# yfs
yfs是一个分布式文件系统

**Lab1: 完成一个Lock服务器**

* 实验难点：

  实验的难点在于第二个点，如何保证RPC只执行**“at-most-once”**（对于读操作来说，RPC可以执行多次，但对于写操作来说，可能不是幂等的，所以只能保证执行一次）。主要的实现思想还是记录request的id，然后查找是否已经执行过。在lab里rpc的通信采用了如下的格式：

  > ```
  > request format:
  > (TCP/IP headers...)
  >   xid
  >   proc#
  >   client nonce
  >   server nonce
  >   xid_rep
  >   arguments...
  > 
  > response format:
  > (TCP/IP headers...)
  >   xid
  >   error code
  >   return value
  > ```

  ​	这里的xid表示此次发送的request的id，而xid_rep表示的是client收到的连续xid中的最大值（这里类比TCP的滑动窗口机制），主要想解决的问题是rpc请求时可能出现丢包、重传、延迟等现象，采用这种滑动窗口的机制能够解决此类问题。

* 实验要点：

  1. rpcc::call1函数，call1函数处理client端发送rpc，主要注意以下几个地方：

     > 如何处理msg的？
     >
     > xid_rep = xid_rep_window_.front(); ?
     >
     > 当xid得到response时，update_xid_rep(xid); ?	看懂update_xid_rep函数就能明白滑动窗口的作用

  2. rpcc::got_pdu，got_pdu是rpcc的回调函数，当tcp报文到达时，网络连接层会调用got_pdu

  3. rpcs::got_pdu，got_pdu是rpcs的回调函数，当tcp报文到达时，网络连接层会调用got_pdu

  4. checkduplicate_and_update，该函数是实验要补充的代码，解决rpc执行**“at-most-once”**的问题，其思路如下：

     ```
     checkduplicate_and_update(cn, xid, xid_rep, &b, &sz)
     	must keep, for each cn, state about each xid(必须要保存每个client的每个xid请求的状态)
     	if state[cn/xid] == INPROGRESS
     		RETURN INPROGRESS
     	if state[cn/xid] == DONE
     		RETURN (DONE, kept b and sz)
     	if xid <= previous xid_rep
     		RETURN FORGOTTEN
     	else
     		state[cn/xid] = INPROGRESS
     		RETURN NEW
     ```

  5. add_reply，同样该函数也是实验要补充的代码，其作用是rpc server将运行的结果保存起来

     ```
     add_reply(cn, xid, b, sz)
     	checkduplicate_and_update already set s[cn/xid] = INPROGRESS
     	state[cn/xid] = DONE
     	keep b and sz
     ```

     

  