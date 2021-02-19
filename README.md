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



**Lab2：使用FUSE完成一个基本的文件服务器**

* 实验难点：

  1. 理解fuse.cc代码的作用；
  2. yfs_client.{cc,h}的接口
  3. extent_server和extent_client的作用，extent_server实现了一个KV，并提供了相应的接口给extent_client利用RPC调用

* 实验要点：

   1. fuse.cc代码分析

      ​	fuse.cc主要提供了对FUSE进行操作的函数，用户把某个文件夹进行mount后，通过命令行输入将文件的操作反馈到fuse.cc编译好的进程。

      ​	在fuse.cc的代码主要包含了几个关于文件的操作函数：

      ```c++
      fuseserver_oper.getattr    = fuseserver_getattr;
      fuseserver_oper.statfs     = fuseserver_statfs;
      fuseserver_oper.readdir    = fuseserver_readdir;
      fuseserver_oper.lookup     = fuseserver_lookup;
      fuseserver_oper.create     = fuseserver_create;
      fuseserver_oper.mknod      = fuseserver_mknod;
      fuseserver_oper.open       = fuseserver_open;
      fuseserver_oper.read       = fuseserver_read;
      fuseserver_oper.write      = fuseserver_write;
      fuseserver_oper.setattr    = fuseserver_setattr;
      fuseserver_oper.unlink     = fuseserver_unlink;
      fuseserver_oper.mkdir      = fuseserver_mkdir;
      ```

      ​	上述代码块的目的是实现FUSE文件系统的操作绑定，比如在FUSE文件系统执行写（write）操作时，那么后台的fuse进程会调用fuseserver_write函数。此外，fuseserver_xxx是文件操作的具体实现方法，以fuseserver_write为例：

      ```c++
      void
      fuseserver_write(fuse_req_t req, fuse_ino_t ino,
        const char *buf, size_t size, off_t off,
        struct fuse_file_info *fi)
      {
        // You fill this in for Lab 2
      #if 1
        // Change the above line to "#if 1", and your code goes here
        yfs_client::inum inum = ino;
      
        if (yfs->write(inum, off, size, buf) != extent_protocol::OK) {
          fuse_reply_err(req, ENOENT);
          return;
        }
        
        fuse_reply_write(req, size);
      #else
        fuse_reply_err(req, ENOSYS);
      #endif
      }
      ```

      ​	fuseserver_write最终会调用yfs->write函数来完成真正的文件写操作，而yfs是一个yfs_client的客户端 `yfs_client *yfs`

   2. yfs_client.{cc,h}代码分析

      ​	yfs_client是yfs文件系统的客户端，内部包含了extent_client结构，extent_client通过RPC与extent_server进行通信，完成文件的操作。

      ​	yfs_client的实现主要如下：

      ```c++
      class yfs_client {
        extent_client *ec;
       public:
      
        typedef unsigned long long inum;
        enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
        typedef int status;
      
        struct fileinfo {
          unsigned long long size;
          unsigned long atime;
          unsigned long mtime;
          unsigned long ctime;
        };
        struct dirinfo {
          unsigned long atime;
          unsigned long mtime;
          unsigned long ctime;
        };
        struct dirent {
          std::string name;
          yfs_client::inum inum;
        };
      
       private:
        static std::string filename(inum);
        static inum n2i(std::string);
        inum random_inum(bool isfile);
      
       public:
      
        yfs_client(std::string, std::string);
      
        bool isfile(inum);
        bool isdir(inum);
      
        int getfile(inum, fileinfo &);
        int getdir(inum, dirinfo &);
      
        int create(inum, const char*, inum &);
        int read(inum, int, int, std::string &);
        int write(inum, int, int, const char*);
        int lookup(inum, const char*, inum &, bool*);
      
        int setattr(inum, struct stat*);
        int readdir(inum, std::list<dirent>&);
      };
      ```

   3. extent_server.{cc,h}代码分析

      ​	extent_server是文件存储的服务器，对外提供了KV存储的接口，文件和元数据以KV的形式存储：

      ```c++
   struct extent {
        // data
        std::string data;
        // attribute
        extent_protocol::attr attr;
      };
      
      class extent_server {
      
      public:
        extent_server();
      
        int put(extent_protocol::extentid_t id, std::string, int &);
        int get(extent_protocol::extentid_t id, std::string &);
        int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
        int remove(extent_protocol::extentid_t id, int &);
      
      private:
        std::mutex mtx;   // protect dataMap
        std::map<extent_protocol::extentid_t, extent> dataMap;
      };
      ```
   



**Lab3：完成MKDIR, UNLINK的功能，以及Locking**

 * 实验难点

   1. 完成mkdir和unlink的语义，与lab2的内容相同；
   2. 考虑在操作文件系统时，出现的并发问题，比如两个client同时写文件，会出现"last writer wins"的现象

 * 实验要点

   1. 在create、write等“写”操作时，需要利用Lab1的Lock_Server进行加锁，因此可以在yfs_client的结构中加入lock_client，与lock_server进行通信：

      ```c++
      class yfs_client {
        extent_client *ec;
        lock_client *lc;    // lock server
       public:
      	xxxx
      };
      ```

   2. 在加锁的过程可是使用类似STL的lock_guard方式，即RAII（资源获取即初始化）：

      ```c++
      class LockGuard {
      public:
        LockGuard() = default;
        
        LockGuard(lock_client *lc, lock_protocol::lockid_t lid) : m_lc(lc), m_lid(lid) {
          m_lc->acquire(m_lid);
        }
      
        ~LockGuard() {
          m_lc->release(m_lid);
        }
      
      private:
        lock_client *m_lc;
        lock_protocol::lockid_t m_lid;
      };
      ```



**Lab4：实现锁的缓存**

* 实验难点：

  1. 设计锁的缓存机制，即客户端在获取锁时，为了降低网络的负载，将锁缓存在客户端本地
  2. 客户端对锁的缓存，以及归还服务器的问题
  3. 不同客户端对锁的竞争

* 实验要点：

  1. 客户端的协议设计：
  
     根据实验材料的提示，客户端对于每一个锁，都有如下的状态：
  
     ```c++
     enum lock_state {
         NONE,
         FREE,
         LOCKED,
         ACQUIRING,
         RELEASING
     };
     ```
  
     NONE表示当前的客户端对锁无感知（锁不在客户端本地）；FREE表示当前锁在本地，且没有线程加锁；LOCKED表示锁在本地，且有线程持有；ACCQUIRING表示当前的客户端还未获取到锁，并且在向锁服务器申请锁资源；RELEASING表示当前的客户端将锁归还给锁服务器。
  
  2. 服务端的协议设计：
  
     ​	下面是在实验过程中的一个错误协议，其思路就是使用了FREE和LOCKED的两个状态，当在调用acquire获取锁时，如果发现锁的状态是LOCKED说明当前的锁被其它客户端占有，需要向该客户端发送revoke的RPC；当该客户端释放锁时，锁服务器向等待的客户端发送retry的RPC。
  
  ```c++
  int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                                 int &)
  {
    lock_protocol::status ret = lock_protocol::OK;
    bool revoke = false;
    std::unique_lock<std::mutex> lck(mtx);
  
    auto iter = lockMap.find(lid);
    if (iter == lockMap.end()) {
      iter = lockMap.insert(std::make_pair(lid, lock_entry())).first;
      iter->second.owner = id;
      iter->second.state = LOCKED;
    } else {
      if (iter->second.state == FREE) {
        iter->second.owner = id;
        // 从waitSet中删除id
        iter->second.waitSet.erase(id);
        // 改变lock的状态
        iter->second.state = LOCKED;
      } else {
        if (iter->second.owner != id) {
          revoke = true;
          iter->second.waitSet.insert(id);
          ret = lock_protocol::RETRY;
        }
        
      }
    }
    lck.unlock();
  
    if (revoke) {
      int r;
      handle(iter->second.owner).safebind()->call(rlock_protocol::revoke, lid, r);
    }
  
    return ret;
  }
  
  int 
  lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
           int &r)
  {
    lock_protocol::status ret = lock_protocol::OK;
  
    std::unique_lock<std::mutex> lck(mtx);
  
    auto iter = lockMap.find(lid);
    if (iter == lockMap.end()) {
      return lock_protocol::IOERR;
    }
    if (iter->second.owner != id) {
      return lock_protocol::IOERR;
    }
  // release
    iter->second.owner = "";
    iter->second.state = FREE;
    
    if (!iter->second.waitSet.empty()) {
      // send retryRPC
      std::string retryClient = *(iter->second.waitSet.begin());
      // unlock before rpc
      lck.unlock();
      handle(retryClient).safebind()->call(rlock_protocol::retry, lid, r);
    }
  
    return ret;
  }
  ```
  
  ​	该方案有个bug会导致deadlock：假设A，B，C三个客户端向服务器申请同一把锁，此时A先来，获得了锁，而B和C后来，分别等待，并且服务器向A发送了revoke。当A完成任务后，将锁归还给服务器，此时服务器将选择B发送retry，而当B得到了锁后，就会将锁缓存在本地，那么C将无法获得锁，除非有新的客户端获取锁，才可能触发服务器向B发送revoke。所以C将一直阻塞。
  
  ​	那么一个解决的方法就是将**状态划分的更细**。具体地，服务器端锁的状态可以划分为：
  
  ```c++
  enum lock_state {
      FREE,
      LOCKED,
      // 为了防止出现deadlock，需要以下两个状态
      LOCKED_AND_WAIT,  // 表示有其它客户端等待锁
      RETRYING          // 表示服务器提醒客户端重试
  };
  ```
  
  ​	此处增加了LOCKED_AND_WAIT和RETRYING的字段，为的就是区分锁被某个客户端单独使用，还是有其它的客户端在等待。例如在客户端在获取锁时，如果当前锁的状态是LOCKED或LOCKED_AND_WAIT，那么需要等待，并且服务器要向锁的持有者发送revoke



**Lab5：实现文件缓存**

* 实验难点

  1. 为了减少网络的负载，extent_client需要将extent_server的数据进行缓存
  2. 不同的extent_client需要实现一致性，例如A修改了data，并缓存在A的本地，那么B要访问data时，A对data的修改要对B可见

* 实验要点

  1. 数据缓存需要保存一些状态，比如数据是否更新，如果数据更新了，需要有write-back的机制，保证extent_server能够有最新的数据：

     ```c++
     enum extent_state {
         NONE,
         CACHED,     // 表示缓存未修改
         UPDATED,    // 缓存已更新
         REMOVED     // 缓存已删除
     };
     ```

  2. 关于extent_client的一致性实现，采用的是向lock_server获取锁，因此extent_id和lock_id采用相同的编号。extent_client的结构如下：

     ```c++
     class extent_client {
     public:
       extent_client(std::string dst);
     
       extent_protocol::status get(extent_protocol::extentid_t eid,
                                     std::string &buf);
       extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                       extent_protocol::attr &a);
       extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
       extent_protocol::status remove(extent_protocol::extentid_t eid);
       extent_protocol::status flush(extent_protocol::extentid_t eid);
     
     private:
         rpcc *cl;
         enum extent_state {
             NONE,
             CACHED,     // 表示缓存未修改
             UPDATED,    // 缓存已更新
             REMOVED     // 缓存已删除
         };
     
         struct extent_entry {
             std::string data;
             extent_protocol::attr attr;
             extent_state state;
     
             extent_entry() : state(NONE) {}
         };
         
         std::unordered_map<extent_protocol::extentid_t, extent_entry> extentCache;
         std::mutex mtx;
     };
     ```

     在yfs_client中，需要有extent_client和lock_client_cache两个客户端，lock_client_cache在进行release的过程中，需要判断是否将缓存的数据写回到extent_server上再释放锁：

     ```c++
     // Classes that inherit lock_release_user can override dorelease so that 
     // that they will be called when lock_client releases a lock.
     // You will not need to do anything with this class until Lab 5.
     class lock_release_user {
      public:
       virtual void dorelease(lock_protocol::lockid_t) = 0;
       virtual ~lock_release_user() {};
     };
     
     class lock_user : public lock_release_user {
     public:
       lock_user(extent_client *e) : ec(e) {}
       // dorelease在将锁释放回服务器时调用
       void dorelease(lock_protocol::lockid_t lid) {
         ec->flush(lid);
       }
     
     private:
       extent_client *ec;
     };
     ```

     而在lock_client_cache的类中，保存着lock_release_user类：

     ```c++
     class lock_client_cache : public lock_client {
      private:
       class lock_release_user *lu;
       int rlock_port;
       std::string hostname;
       std::string id;
       xxxxxx
     };
     ```




**Lab6：实现Paxos协议**

* 实验难点：

  1. 之前的实验中并未考虑lock_server单点故障的问题，在Lab6中将实现Paxos一致性算法解决该问题
  2. 理解RSM的思路——机器初始状态相同，那么执行相同的操作系列后状态也是相同的。由于网络乱序等原因，无法保证所有备份机器收到的操作请求序列都是相同的，所以采用一机器为master，master从客户端接受请求,决定请求次序，然后发送给各个备份机器，然后以相同的次序在所有备份(replicas)机器上执行，master等待所有备份机器返回，然后master返回给客户端，当master失败，任何一个备份(replicas)可以接管工作，因为他们都有相同的状态。

* 实验要点：

  1. 理解config.{h,cc}、rsm.{h,cc}和paxos.{h,cc}模块的关系：rsm是复制状态机，属于最上层的模块，而rsm的模块下有config，config表示对不同server节点的管理，主要是通过心跳包的发送和接收管理宕机节点。而config模块下有paxos，paxos实现状态的一致性，具体的结构如下：

     ```c++
     class rsm : public config_view_change {
      protected:
       ......
       config *cfg;
       ......
     };
     
     class config : public paxos_change {
      private:
       ......
       acceptor *acc;
       proposer *pro;
       ......
     };
     ```

     

  2. 完成paxos.{cc,h}的代码

     paxos有两个角色（proposer和acceptor），需要分别实现两者的逻辑。paxos的伪代码如下所示：

     ```
     proposer run(instance, v):
      choose n, unique and higher than any n seen so far
      send prepare(instance, n) to all servers including self
      if oldinstance(instance, instance_value) from any node:
        commit to the instance_value locally
      else if prepare_ok(n_a, v_a) from majority:
        v' = v_a with highest n_a; choose own v otherwise
        send accept(instance, n, v') to all
        if accept_ok(n) from majority:
          send decided(instance, v') to all
     
     acceptor state:
      must persist across reboots
      n_h (highest prepare seen)
      instance_h, (highest instance accepted)
      n_a, v_a (highest accept seen)
     
     acceptor prepare(instance, n) handler:
      if instance <= instance_h
        reply oldinstance(instance, instance_value)
      else if n > n_h
        n_h = n
        reply prepare_ok(n_a, v_a)
      else
        reply prepare_reject
     
     acceptor accept(instance, n, v) handler:
      if n >= n_h
        n_a = n
        v_a = v
        reply accept_ok(n)
      else
        reply accept_reject
     
     acceptor decide(instance, v) handler:
      paxos_commit(instance, v)
     ```

     在伪码里，“instance”表示一次paxos达成一致的实例，因为在原始论文中，paxos只能对一个value达成一致，如果有多个value，则需要用不同的paxos实例。