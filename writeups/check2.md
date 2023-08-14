+ 有几个需要注意的点
  + receive
    + 如果没有收到syn，就是没有建立连接，那么所有的数据都应该丢掉，即使收到fin也丢掉
    + 如果在syn消息中含有payload数据，那么insert的时候first_index要加1，因为该消息的syn占了一个seqno，但却没有真实存在的数据
  + send
    + 如果还没有syn直接置ackno为空
    + ackno=pushed+isn+1+isclosed, syn占1位，如果close了，ackno还需要加1