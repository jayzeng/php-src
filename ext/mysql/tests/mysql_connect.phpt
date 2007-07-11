--TEST--
mysql_connect()
--SKIPIF--
<?php require_once('skipif.inc'); ?>
--FILE--
<?php
include_once "connect.inc";

$tmp    = NULL;
$link   = NULL;

// mysql_connect ( [string server [, string username [, string password [, bool new_link [, int client_flags]]]]] )
if (NULL !== ($tmp = @mysql_connect($link, $link, $link, $link, $link, $link)))
	printf("[001] Expecting NULL, got %s/%s\n", gettype($tmp), $tmp);

$myhost = (is_null($socket)) ? ((is_null($port)) ? $host : $host . ':' . $port) : $host . ':' . $socket;
if (!$link = mysql_connect($myhost, $user, $passwd, true))
	printf("[002] Cannot connect to the server using host=%s/%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
		$host, $myhost, $user, $db, $port, $socket);

mysql_close($link);

if ($link = mysql_connect($myhost, $user . 'unknown_really', $passwd . 'non_empty', true))
	printf("[003] Can connect to the server using host=%s/%s, user=%s, passwd=***non_empty, dbname=%s, port=%s, socket=%s\n",
		$host, $myhost, $user . 'unknown_really', $db, $port, $socket);

if (false !== $link)
printf("[004] Expecting boolean/false, got %s/%s\n", gettype($link), $link);

// Run the following tests without an anoynmous MySQL user and use a password for the test user!
ini_set('mysql.default_socket', $socket);
if (!is_resource($link = mysql_connect($host, $user, $passwd, true))) {
	printf("[005] Usage of mysql.default_socket failed\n");
} else {
	mysql_close($link);
}

if (!ini_get('sql.safe_mode')) {

	ini_set('mysql.default_port', $port);
	if (!is_resource($link = mysql_connect($host, $user, $passwd, true))) {
		printf("[006] Usage of mysql.default_port failed\n");
	} else {
		mysql_close($link);
	}

	ini_set('mysql.default_password', $passwd);
	if (!is_resource($link = mysql_connect($myhost, $user))) {
		printf("[007] Usage of mysql.default_password failed\n");
	} else {
		mysql_close($link);
	}
	ini_set('mysql.default_user', $user);
	if (!is_resource($link = mysql_connect($myhost))) {
		printf("[008] Usage of mysql.default_user failed\n");
	} else {
		mysql_close($link);
	}

	ini_set('mysql.default_host', $myhost);
	if (!is_resource($link = mysql_connect())) {
		printf("[009] Usage of mysql.default_host failed\n") ;
	} else {
		mysql_close($link);
	}

	if (!stristr(PHP_OS, 'win') && !stristr(PHP_OS, 'netware')) {
		ini_set('mysql.default_port', -1);
		if (putenv(sprintf('MYSQL_TCP_PORT=%d', $port))) {
			if (!is_resource($link = mysql_connect())) {
				printf("[010] Usage of env MYSQL_TCP_PORT failed\n") ;
			} else {
			mysql_close($link);
		}
		} else if (putenv(sprintf('MYSQL_TCP_PORT=%d', $port + 1))) {
			if (!is_resource($link = mysql_connect())) {
				printf("[011] Usage of env MYSQL_TCP_PORT=%d should have failed\n", $port + 1) ;
				mysql_close($link);
		}
		}
	}
}

/* we don't care if we can connect or not, this is just to test the different connect syntax variants */
$myhost = sprintf('%s:%d', $host, $port - 100);
if ($link = mysql_connect($myhost, $user, $passwd, true))
	mysql_close($link);

/* interesting, isn't it? */
$myhost = sprintf('%s:%d:%s', $host, $port - 100, $socket);
if ($link = mysql_connect($myhost, $user, $passwd, true))
	mysql_close($link);

print "done!";
?>
--EXPECTF--
Warning: mysql_connect(): Access denied for user '%s'@'%s' (using password: YES) in %s on line %d
done!