php-amqp is a PHP Extension written in C to enable support for AMQP (Advanced Message Queuing Protocol) messaging directly from PHP. The extension is using the rabbitmq library (Version 1.6.0, AMQP Version 0.8) (http://hg.rabbitmq.com/rabbitmq-c).

## Installation ##
  * Download librabbitmq-c from from http://hg.rabbitmq.com/rabbitmq-c/ and extract its contents.
  * Download librabbitmq-codegen from http://hg.rabbitmq.com/rabbitmq-codegen/ and extract its contents to a subdirectory in librabbitmq-c named "codegen"
or if you have mercurial installed:
```
hg clone http://hg.rabbitmq.com/rabbitmq-c
cd rabbitmq-c
hg clone http://hg.rabbitmq.com/rabbitmq-codegen codegen
```
  * Compile and install librabbitmq using:
```
autoreconf -i && ./configure && make && sudo make install
```
  * Download and unpack the PHP extension, compile and install it using:
```
phpize && ./configure --with-amqp && make && sudo make install
```
  * Add the PHP Extension to your php.ini file:
```
extension = amqp.so
```

## Example ##
Check example.php in the source also ...

```
<?php

// create a connection ...
$connection = amqp_connection_popen("localhost", 5672 );

$user = "guest";
$pass = "guest";
$vhost = "/";

// login
$res = amqp_login($connection, $user, $pass, $vhost);

// open channel
$channel_id = 1;
$res = amqp_channel_open($connection, $channel_id);

$queue = 'MyQueue';
$exchange ="myexchange.direct";
$routing_key = "RoutingKey";

// optional: declare exchange, declare queue, bind queue
$res = amqp_exchange_declare($connection, $channel_id, $exchange, "direct");
$res = amqp_queue_declare($connection, $channel_id, $queue, $passive = false, $durable = false, $exclusive = false, $auto_delete = true);
$res = amqp_queue_bind($connection, $channel_id, $queue, $exchange, $routing_key);



// optinal, specify options for basic_publish()
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
$body = "Message Body";
$start = microtime(true);
for ($i = 0; $i < 10; $i++) {
    $res = amqp_basic_publish($connection, $channel_id, $exchange, $routing_key, $body, false, false, $options);
}


$end = microtime(true);
echo "Total publish time: " . ($end - $start) ."\n";


```