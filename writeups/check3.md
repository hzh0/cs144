## TCP sender

### 注意事项
+ windows_size_ 初始默认值为1 不为0？
    + 因为当真实的windows_size_=0时（即真的receive到消息了），在发生超时时，需要重制超时时间为初始值。而最开始发送SYN消息时，此时windows_size为默认值1，如果为0，那么每回都会将超时时间变成初始值，但此时不能变成初始值，需要变成双倍，所以需要注意一下。
+ windows_size_ 确实为0时的处理
  + 发送一个字节的消息，下次再push时，如果windows_size_还是0，上一次发送的那一个字节还没被确认，就直接返回，不要再发一个字节了，上一个字节超时会重发的
+ 开始的syn消息的fin段可能是true，就是这个tcp连接不发送任何实质性消息，直接在建立连接的段发送fin。
+ remain_size_ = max(0, windows_size_-in_flight_num_);为什么要取max，因为windows_size是可能变小的，remain_size可能为负，为负时直接变0不要继续发送就行了
+ 删除被确认的消息时，因为实验将一条消息视为不可分，假如我发送一条消息序列号是 15-20，确认号是17，即已经成功收到了15，16字节，此时不需要将消息分成15-16、17-20,然后删除15-16，直接视整段15-20都没有被确认就好
+ 何时重置连续超时次数和超时时间？
  + 发送一条新消息且计时器没有启动时（有可能上一条发送的消息还没有确认，此时不需要重置计时器）
  + recv的序列号大于之前时
+ 特别需要注意：每一次时钟tick时，如果windows_size为0，那么需要重置超时时间，除了第一条syn消息，原因如上。
### 代码逻辑

+ optional<TCPSenderMessage> TCPSender::maybe_send() 逻辑：
  ```cpp
  if 超时:
    重启计时器(不重置超时时间)，重发最开始发送的消息
  else:
    if 存在未发送的消息：
        重启计时器(重置超时时间)，发送一条未发送的消息
    else
        返回 nullopt
  ```
+ void TCPSender::push( Reader& outbound_stream ) 逻辑
  ```cpp
  if fin_(已经发送过了最后一个消息) 直接返回
  if !syn_(还没建立连接):
    发送一条syn消息(需要注意:如果outbound_stream已经结束了，该syn消息的fin为true)
    返回
  if windows_size = 0:
    if 发送和待发送消息为0(防止连续为0不断push一字段消息):
        remain_size_ = 1
  
  if remain_size_ 为 0:
    返回
  
  len = min(outbound_stream的剩余字节数，remain_size_)
  
  while (len > TCPConfig::MAX_PAYLOAD_SIZE):
    不断产生 payload为TCPConfig::MAX_PAYLOAD_SIZE大小的消息，且syn和fin一直为fasle(fin为什么不能为true？因为len都大于TCPConfig::MAX_PAYLOAD_SIZE了，后面肯定还有数据)
    len -= TCPConfig::MAX_PAYLOAD_SIZE
  组装剩下的len字节数据
  ```
+ TCPSenderMessage TCPSender::send_empty_message() const 逻辑
  + 发送一条消息，isn为下一个要正确发送的消息的isn，syn=fasle,payload="",fin=false
+ void TCPSender::receive( const TCPReceiverMessage& msg )
  ```cpp
  if !syn || msg的ack为空 || msg的ack比之前收到过的ack要小 || msg的ack比发送的最后一个isn还要大
    都属于异常情况需要直接返回
  if msg的ack不同于上一次的ack，即接收方又确认了一些新的消息，那么需要重置超时时间，连续超时次数也要重置为0
  
  更新 windows_size_ = msg.window_size;
  更新 remain_size_ = max(0, windows_size_-in_flight_num_);
  更新 last_ack_isn_ = msg.ackno.value();
  
  更新，删掉已经确认过的消息，只要删除了消息就将当前时钟置0，重新计时
  如果删除完了所有发送消息，停止计时器，如果没有删除完，确保计时器仍在继续计时
  ```