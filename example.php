<?php
ini_set("extension_dir", dirname(__FILE__).'/modules');
dl("amqp.so");


if (!extension_loaded("amqp")) {
    die("extension amqp not loaded!\n");
}


$hostname = "localhost";
$port     = 5672;

// create a connection ...
$connection = amqp_new_connection($hostname, 5672 );

$user = "guest";
$pass = "guest";
$vhost = "/";
$channel_max = 0;
$frame_max = 131072;
$sasl_method = 0;

// login ...
$res = amqp_login($connection, $user, $pass, $vhost, $channel_max, $frame_max, $sasl_method);


$channel_id = 1;
$exchange ="amq.direct";
$routing_key = "test queue";
$body = "";

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
for ($i = 0; $i < 1; $i++) {
    $res = amqp_basic_publish($connection, $channel_id, $exchange, $routing_key, $body, false, false, $options);
}

$end = microtime(true);
echo "Total publish time: " . ($end - $start) ."\n";

