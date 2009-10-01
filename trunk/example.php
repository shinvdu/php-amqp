<?php
error_reporting(E_ALL);
var_dump(get_included_files());

restore_exception_handler();
restore_error_handler();
// xdebug_disable();

ini_set("extension_dir", dirname(__FILE__).'/modules');
if (!extension_loaded("amqp")) {
    dl("amqp.so");
}

if (!extension_loaded("amqp")) {
    die("extension amqp not loaded!\n");
}


$hostname = "localhost";
$port     = 5672;

// create a connection ...
$connection = amqp_connection_popen($hostname, 5672 );

if (!is_resource($connection)) {
   var_dump($connection);
   die("connection failed");
}

$user = "guest";
$pass = "guest";
$vhost = "/";
$channel_max = 0;
$frame_max = 131072;
$sasl_method = 0;
$queue = 'FeedQueue';


// login ...
$res = amqp_login($connection, $user, $pass, $vhost, $channel_max, $frame_max, $sasl_method);

$channel_id = 1;
$res = amqp_channel_open($connection, 1);

$exchange ="feed.direct";
$routing_key = "TestRoutingKey";
$body = "";


$y = amqp_exchange_declare($connection, $channel_id, $exchange, "direct");


$y = amqp_queue_declare($connection, $channel_id, $queue, $passive = false, $durable = false, $exclusive = false, $auto_delete = true);


$y = amqp_queue_bind($connection, $channel_id, $queue, $exchange, $routing_key);
var_dump($y);


$y = amqp_queue_unbind($connection, $channel_id, $queue, $exchange, $routing_key);
var_dump($y);

// die('ende');
var_dump( $queue, $exchange, $routing_key);

// create a test message
for ($i = 0; $i < 256; $i++) {
    $body .= (string) $i & 0xff;
}

$body = "Hier ist mein DemoContent";

$options = array(
  "delivery_mode" => 2,
  "content_type" => "text/plain",
  "content_encoding" => "utf-8"
);


$options = array(
    "content_type" => "ContentType",
    "content_encoding" => "ContentEncoding",
    "delivery_mode" => 2,
    "priority" => 1,
    "correlation_id" => "correlation_id",
    "reply_to" => "reply_to",
    "expiration" => "tomorowww",
    "message_id" => "id of the message",
    "timestamp" => time(),
    "type" => "type of the message",
     "user_id" => "userId!",
     "app_id" => "ApplicationId",
     "cluster_id" => "ClusterId!"
);

// send the message to rabbitmq
$start = microtime(true);
for ($i = 0; $i < 100; $i++) {
    $res = amqp_basic_publish($connection, $channel_id, $exchange, $routing_key, $body, false, false, $options);
sleep(1);
echo ".";
}

$end = microtime(true);
echo "Total publish time: " . ($end - $start) ."\n";



