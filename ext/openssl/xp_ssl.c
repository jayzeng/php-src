/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2004 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Wez Furlong <wez@thebrainroom.com>                           |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "php.h"
#include "ext/standard/file.h"
#include "streams/php_streams_int.h"
#include "php_network.h"
#include "php_openssl.h"
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>

int php_openssl_apply_verification_policy(SSL *ssl, X509 *peer, php_stream *stream TSRMLS_DC);
SSL *php_SSL_new_from_context(SSL_CTX *ctx, php_stream *stream TSRMLS_DC);

/* This implementation is very closely tied to the that of the native
 * sockets implemented in the core.
 * Don't try this technique in other extensions!
 * */

typedef struct _php_openssl_netstream_data_t {
	php_netstream_data_t s;
	SSL *ssl_handle;
	int enable_on_connect;
	int is_client;
	int ssl_active;
	php_stream_xport_crypt_method_t method;
} php_openssl_netstream_data_t;

php_stream_ops php_openssl_socket_ops;

static int handle_ssl_error(php_stream *stream, int nr_bytes TSRMLS_DC)
{
	php_openssl_netstream_data_t *sslsock = (php_openssl_netstream_data_t*)stream->abstract;
	int err = SSL_get_error(sslsock->ssl_handle, nr_bytes);
	char esbuf[512];
	char *ebuf = NULL, *wptr = NULL;
	size_t ebuf_size = 0;
	unsigned long code;
	int retry = 1;

	switch(err) {
		case SSL_ERROR_ZERO_RETURN:
			/* SSL terminated (but socket may still be active) */
			retry = 0;
			break;
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			/* re-negotiation, or perhaps the SSL layer needs more
			 * packets: retry in next iteration */
			break;
		case SSL_ERROR_SYSCALL:
			if (ERR_peek_error() == 0) {
				if (nr_bytes == 0) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING,
							"SSL: fatal protocol error");
					stream->eof = 1;
					retry = 0;
				} else {
					char *estr = php_socket_strerror(php_socket_errno(), NULL, 0);

					php_error_docref(NULL TSRMLS_CC, E_WARNING,
							"SSL: %s", estr);

					efree(estr);
					retry = 0;
				}
				break;
			}
			/* fall through */
		default:
			/* some other error */
			while ((code = ERR_get_error()) != 0) {
				/* allow room for a NUL and an optional \n */
				if (ebuf) {
					esbuf[0] = '\n';
					esbuf[1] = '\0';
					ERR_error_string_n(code, esbuf + 1, sizeof(esbuf) - 2);
				} else {
					esbuf[0] = '\0';
					ERR_error_string_n(code, esbuf, sizeof(esbuf) - 1);
				}
				code = strlen(esbuf);
				esbuf[code] = '\0';

				ebuf = erealloc(ebuf, ebuf_size + code + 1);
				if (wptr == NULL) {
					wptr = ebuf;
				}	

				/* also copies the NUL */
				memcpy(wptr, esbuf, code + 1);
				wptr += code;
			}

			php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"SSL operation failed with code %d.%s%s",
					err,
					ebuf ? "OpenSSL Error messages:\n" : "",
					ebuf ? ebuf : "");
				
			retry = 0;
	}
	return retry;
}


static size_t php_openssl_sockop_write(php_stream *stream, const char *buf, size_t count TSRMLS_DC)
{
	php_openssl_netstream_data_t *sslsock = (php_openssl_netstream_data_t*)stream->abstract;
	int didwrite;
	
	if (sslsock->ssl_active) {
		int retry = 1;

		do {
			didwrite = SSL_write(sslsock->ssl_handle, buf, count);

			if (didwrite <= 0) {
				retry = handle_ssl_error(stream, didwrite TSRMLS_CC);
			} else {
				break;
			}
		} while(retry);
		
	} else {
		didwrite = php_stream_socket_ops.write(stream, buf, count TSRMLS_CC);
	}
	
	if (didwrite > 0) {
		php_stream_notify_progress_increment(stream->context, didwrite, 0);
	}

	if (didwrite < 0) {
		didwrite = 0;
	}
	
	return didwrite;
}

static size_t php_openssl_sockop_read(php_stream *stream, char *buf, size_t count TSRMLS_DC)
{
	php_openssl_netstream_data_t *sslsock = (php_openssl_netstream_data_t*)stream->abstract;
	int nr_bytes = 0;

	if (sslsock->ssl_active) {
		int retry = 1;

		do {
			nr_bytes = SSL_read(sslsock->ssl_handle, buf, count);

			if (nr_bytes <= 0) {
				retry = handle_ssl_error(stream, nr_bytes TSRMLS_CC);
				stream->eof = (retry == 0 && !SSL_pending(sslsock->ssl_handle));
				
			} else {
				/* we got the data */
				break;
			}
		} while (retry);
	}
	else
	{
		nr_bytes = php_stream_socket_ops.read(stream, buf, count TSRMLS_CC);
	}

	if (nr_bytes > 0) {
		php_stream_notify_progress_increment(stream->context, nr_bytes, 0);
	}

	if (nr_bytes < 0) {
		nr_bytes = 0;
	}

	return nr_bytes;
}


static int php_openssl_sockop_close(php_stream *stream, int close_handle TSRMLS_DC)
{
	php_openssl_netstream_data_t *sslsock = (php_openssl_netstream_data_t*)stream->abstract;
#ifdef PHP_WIN32
	fd_set wrfds, efds;
	int n;
	struct timeval timeout;
#endif
	if (close_handle) {
		if (sslsock->ssl_active) {
			SSL_shutdown(sslsock->ssl_handle);
			sslsock->ssl_active = 0;
		}
		if (sslsock->ssl_handle) {
			SSL_free(sslsock->ssl_handle);
			sslsock->ssl_handle = NULL;
		}
		if (sslsock->s.socket != -1) {
#ifdef PHP_WIN32
			/* prevent more data from coming in */
			shutdown(sslsock->s.socket, SHUT_RD);

			/* try to make sure that the OS sends all data before we close the connection.
			 * Essentially, we are waiting for the socket to become writeable, which means
			 * that all pending data has been sent.
			 * We use a small timeout which should encourage the OS to send the data,
			 * but at the same time avoid hanging indefintely.
			 * */
			do {
				FD_ZERO(&wrfds);
				FD_SET(sslsock->s.socket, &wrfds);
				efds = wrfds;

				timeout.tv_sec = 0;
				timeout.tv_usec = 5000; /* arbitrary */

				n = select(sslsock->s.socket + 1, NULL, &wrfds, &efds, &timeout);
			} while (n == -1 && php_socket_errno() == EINTR);
#endif

			closesocket(sslsock->s.socket);
			sslsock->s.socket = -1;
		}
	}

	pefree(sslsock, php_stream_is_persistent(stream));
	
	return 0;
}

static int php_openssl_sockop_flush(php_stream *stream TSRMLS_DC)
{
	return php_stream_socket_ops.flush(stream TSRMLS_CC);
}

static int php_openssl_sockop_stat(php_stream *stream, php_stream_statbuf *ssb TSRMLS_DC)
{
	return php_stream_socket_ops.stat(stream, ssb TSRMLS_CC);
}


static inline int php_openssl_setup_crypto(php_stream *stream,
		php_openssl_netstream_data_t *sslsock,
		php_stream_xport_crypto_param *cparam
		TSRMLS_DC)
{
	SSL_CTX *ctx;
	SSL_METHOD *method;
	
	if (sslsock->ssl_handle) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "SSL/TLS already set-up for this stream");
		return -1;
	}

	/* need to do slightly different things, based on client/server method,
	 * so lets remember which method was selected */

	switch (cparam->inputs.method) {
		case STREAM_CRYPTO_METHOD_SSLv23_CLIENT:
			sslsock->is_client = 1;
			method = SSLv23_client_method();
			break;
		case STREAM_CRYPTO_METHOD_SSLv2_CLIENT:
			sslsock->is_client = 1;
			method = SSLv2_client_method();
			break;
		case STREAM_CRYPTO_METHOD_SSLv3_CLIENT:
			sslsock->is_client = 1;
			method = SSLv3_client_method();
			break;
		case STREAM_CRYPTO_METHOD_TLS_CLIENT:
			sslsock->is_client = 1;
			method = TLSv1_client_method();
			break;
		case STREAM_CRYPTO_METHOD_SSLv23_SERVER:
			sslsock->is_client = 0;
			method = SSLv23_server_method();
			break;
		case STREAM_CRYPTO_METHOD_SSLv3_SERVER:
			sslsock->is_client = 0;
			method = SSLv3_server_method();
			break;
		case STREAM_CRYPTO_METHOD_SSLv2_SERVER:
			sslsock->is_client = 0;
			method = SSLv2_server_method();
			break;
		case STREAM_CRYPTO_METHOD_TLS_SERVER:
			sslsock->is_client = 0;
			method = TLSv1_server_method();
			break;
		default:
			return -1;

	}

	ctx = SSL_CTX_new(method);
	if (ctx == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to create an SSL context");
		return -1;
	}

	sslsock->ssl_handle = php_SSL_new_from_context(ctx, stream TSRMLS_CC);
	if (sslsock->ssl_handle == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to create an SSL handle");
		SSL_CTX_free(ctx);
		return -1;
	}

	if (!SSL_set_fd(sslsock->ssl_handle, sslsock->s.socket)) {
		handle_ssl_error(stream, 0 TSRMLS_CC);
	}

	if (cparam->inputs.session) {
		if (cparam->inputs.session->ops != &php_openssl_socket_ops) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "supplied session stream must be an SSL enabled stream");
		} else {
			SSL_copy_session_id(sslsock->ssl_handle, ((php_openssl_netstream_data_t*)cparam->inputs.session->abstract)->ssl_handle);
		}
	}
	return 0;
}

static inline int php_openssl_enable_crypto(php_stream *stream,
		php_openssl_netstream_data_t *sslsock,
		php_stream_xport_crypto_param *cparam
		TSRMLS_DC)
{
	int n, retry = 1;

	if (cparam->inputs.activate && !sslsock->ssl_active) {
		if (sslsock->is_client) {
			SSL_set_connect_state(sslsock->ssl_handle);
		} else {
			SSL_set_accept_state(sslsock->ssl_handle);
		}
	
		do {
			if (sslsock->is_client) {
				n = SSL_connect(sslsock->ssl_handle);
			} else {
				n = SSL_accept(sslsock->ssl_handle);
			}

			if (n <= 0) {
				retry = handle_ssl_error(stream, n TSRMLS_CC);
			} else {
				break;
			}
		} while (retry);

		if (n == 1) {
			X509 *peer_cert;

			peer_cert = SSL_get_peer_certificate(sslsock->ssl_handle);

			if (FAILURE == php_openssl_apply_verification_policy(sslsock->ssl_handle, peer_cert, stream TSRMLS_CC)) {
				SSL_shutdown(sslsock->ssl_handle);
			} else {	
				sslsock->ssl_active = 1;
			}

			X509_free(peer_cert);
		}

		return n;

	} else if (!cparam->inputs.activate && sslsock->ssl_active) {
		/* deactivate - common for server/client */
		SSL_shutdown(sslsock->ssl_handle);
		sslsock->ssl_active = 0;
	}
	return -1;
}

static inline int php_openssl_tcp_sockop_accept(php_stream *stream, php_openssl_netstream_data_t *sock,
		php_stream_xport_param *xparam STREAMS_DC TSRMLS_DC)
{
	int clisock;

	xparam->outputs.client = NULL;

	clisock = php_network_accept_incoming(sock->s.socket,
			xparam->want_textaddr ? &xparam->outputs.textaddr : NULL,
			xparam->want_textaddr ? &xparam->outputs.textaddrlen : NULL,
			xparam->want_addr ? &xparam->outputs.addr : NULL,
			xparam->want_addr ? &xparam->outputs.addrlen : NULL,
			xparam->inputs.timeout,
			xparam->want_errortext ? &xparam->outputs.error_text : NULL,
			&xparam->outputs.error_code
			TSRMLS_CC);

	if (clisock >= 0) {
		php_openssl_netstream_data_t *clisockdata;

		clisockdata = pemalloc(sizeof(*clisockdata), stream->is_persistent);

		if (clisockdata == NULL) {
			closesocket(clisock);
			/* technically a fatal error */
		} else {
			/* copy underlying tcp fields */
			memset(clisockdata, 0, sizeof(*clisockdata));
			memcpy(clisockdata, sock, sizeof(clisockdata->s));

			clisockdata->s.socket = clisock;
			
			xparam->outputs.client = php_stream_alloc_rel(stream->ops, clisockdata, NULL, "r+");
			if (xparam->outputs.client) {
				xparam->outputs.client->context = stream->context;
			}
		}
	}
	
	return xparam->outputs.client == NULL ? -1 : 0;
}
static int php_openssl_sockop_set_option(php_stream *stream, int option, int value, void *ptrparam TSRMLS_DC)
{
	php_openssl_netstream_data_t *sslsock = (php_openssl_netstream_data_t*)stream->abstract;
	php_stream_xport_crypto_param *cparam = (php_stream_xport_crypto_param *)ptrparam;
	php_stream_xport_param *xparam = (php_stream_xport_param *)ptrparam;

	switch (option) {
		case PHP_STREAM_OPTION_CHECK_LIVENESS:
			{
				fd_set rfds;
				struct timeval tv;
				char buf;
				int alive = 1;

				if (sslsock->s.timeout.tv_sec == -1) {
					tv.tv_sec = FG(default_socket_timeout);
				} else {
					tv = sslsock->s.timeout;
				}

				if (sslsock->s.socket == -1) {
					alive = 0;
				} else {
					FD_ZERO(&rfds);
					FD_SET(sslsock->s.socket, &rfds);

					if (select(sslsock->s.socket + 1, &rfds, NULL, NULL, &tv) > 0 && FD_ISSET(sslsock->s.socket, &rfds)) {
						if (sslsock->ssl_active) {
							int n;

							do {
								n = SSL_peek(sslsock->ssl_handle, &buf, sizeof(buf));
								if (n <= 0) {
									int err = SSL_get_error(sslsock->ssl_handle, n);

									if (err == SSL_ERROR_SYSCALL) {
										alive = php_socket_errno() == EAGAIN;
										break;
									}

									if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
										/* re-negotiate */
										continue;
									}

									/* any other problem is a fatal error */
									alive = 0;
								}
								/* either peek succeeded or there was an error; we
								 * have set the alive flag appropriately */
								break;
							} while (1);
						} else if (0 == recv(sslsock->s.socket, &buf, sizeof(buf), MSG_PEEK) && php_socket_errno() != EAGAIN) {
							alive = 0;
						}
					}
				}
				return alive ? PHP_STREAM_OPTION_RETURN_OK : PHP_STREAM_OPTION_RETURN_ERR;
			}
			
		case PHP_STREAM_OPTION_CRYPTO_API:

			switch(cparam->op) {

				case STREAM_XPORT_CRYPTO_OP_SETUP:
					cparam->outputs.returncode = php_openssl_setup_crypto(stream, sslsock, cparam TSRMLS_CC);
					return PHP_STREAM_OPTION_RETURN_OK;
					break;
				case STREAM_XPORT_CRYPTO_OP_ENABLE:
					cparam->outputs.returncode = php_openssl_enable_crypto(stream, sslsock, cparam TSRMLS_CC);
					return PHP_STREAM_OPTION_RETURN_OK;
					break;
				default:
					/* fall through */
					break;
			}

			break;

		case PHP_STREAM_OPTION_XPORT_API:
			switch(xparam->op) {

				case STREAM_XPORT_OP_CONNECT:
				case STREAM_XPORT_OP_CONNECT_ASYNC:
					/* TODO: Async connects need to check the enable_on_connect option when
					 * we notice that the connect has actually been established */
					php_stream_socket_ops.set_option(stream, option, value, ptrparam TSRMLS_CC);

					if (xparam->outputs.returncode == 0 && sslsock->enable_on_connect) {
						if (php_stream_xport_crypto_setup(stream, sslsock->method, NULL TSRMLS_CC) < 0 ||
								php_stream_xport_crypto_enable(stream, 1 TSRMLS_CC) < 0) {
							php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to enable crypto");
							xparam->outputs.returncode = -1;
						}
					}
					return PHP_STREAM_OPTION_RETURN_OK;
					break;

				case STREAM_XPORT_OP_ACCEPT:
					/* we need to copy the additional fields that the underlying tcp transport
					 * doesn't know about */
					xparam->outputs.returncode = php_openssl_tcp_sockop_accept(stream, sslsock, xparam STREAMS_CC TSRMLS_CC);
					return PHP_STREAM_OPTION_RETURN_OK;
					break;

				default:
					/* fall through */
					break;
			}
	}

	return php_stream_socket_ops.set_option(stream, option, value, ptrparam TSRMLS_CC);
}

static int php_openssl_sockop_cast(php_stream *stream, int castas, void **ret TSRMLS_DC)
{
	php_openssl_netstream_data_t *sslsock = (php_openssl_netstream_data_t*)stream->abstract;

	switch(castas)	{
		case PHP_STREAM_AS_STDIO:
			if (sslsock->ssl_active) {
				return FAILURE;
			}
			if (ret)	{
				*ret = fdopen(sslsock->s.socket, stream->mode);
				if (*ret) {
					return SUCCESS;
				}
				return FAILURE;
			}
			return SUCCESS;

		case PHP_STREAM_AS_FD_FOR_SELECT:
			if (ret) {
				*ret = (void*)sslsock->s.socket;
			}
			return SUCCESS;

		case PHP_STREAM_AS_FD:
		case PHP_STREAM_AS_SOCKETD:
			if (sslsock->ssl_active) {
				return FAILURE;
			}
			if (ret) {
				*ret = (void*)sslsock->s.socket;
			}
			return SUCCESS;
		default:
			return FAILURE;
	}
}

php_stream_ops php_openssl_socket_ops = {
	php_openssl_sockop_write, php_openssl_sockop_read,
	php_openssl_sockop_close, php_openssl_sockop_flush,
	"tcp_socket/ssl",
	NULL, /* seek */
	php_openssl_sockop_cast,
	php_openssl_sockop_stat,
	php_openssl_sockop_set_option,
};


php_stream *php_openssl_ssl_socket_factory(const char *proto, long protolen,
		char *resourcename, long resourcenamelen,
		const char *persistent_id, int options, int flags,
		struct timeval *timeout,
		php_stream_context *context STREAMS_DC TSRMLS_DC)
{
	php_stream *stream = NULL;
	php_openssl_netstream_data_t *sslsock = NULL;
	
	sslsock = pemalloc(sizeof(php_openssl_netstream_data_t), persistent_id ? 1 : 0);
	memset(sslsock, 0, sizeof(*sslsock));

	sslsock->s.is_blocked = 1;
	sslsock->s.timeout.tv_sec = FG(default_socket_timeout);
	sslsock->s.timeout.tv_usec = 0;

	/* we don't know the socket until we have determined if we are binding or
	 * connecting */
	sslsock->s.socket = -1;
	
	stream = php_stream_alloc_rel(&php_openssl_socket_ops, sslsock, persistent_id, "r+");

	if (stream == NULL)	{
		pefree(sslsock, persistent_id ? 1 : 0);
		return NULL;
	}

	if (strncmp(proto, "ssl", protolen) == 0) {
		sslsock->enable_on_connect = 1;
		sslsock->method = STREAM_CRYPTO_METHOD_SSLv23_CLIENT;
	} else if (strncmp(proto, "tls", protolen) == 0) {
		sslsock->enable_on_connect = 1;
		sslsock->method = STREAM_CRYPTO_METHOD_TLS_CLIENT;
	}

	return stream;
}



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
