nanoRPC
=======

nanoRPC 是我（[yangliangwei](https://github.com/yangliangwei0623)）的个人 C++ RPC 项目，用于学习、实践和持续改进 RPC 的实现。

本项目参考并基于 [Remmy](https://github.com/hjk41/Remmy) 开发，感谢原项目提供的实现。当前使用 C++17，支持 ASIO 和 ZeroMQ 通信层。

目前已在 Linux 下验证 ASIO 示例。源码目录、命名空间和示例程序仍沿用 `remmy` 命名。

Linux 本地运行（ASIO）
-------

需要支持 C++17 的编译器、Make 和 CMake 3.21 或更新版本。Ubuntu / Debian 可安装：

```bash
sudo apt update
sudo apt install build-essential cmake
```

在项目根目录执行：

```bash
git submodule update --init --recursive
cmake --preset asio
cmake --build --preset asio
./build/asio/remmy_test
```

`asio` 预设使用仓库内的 ASIO 依赖，启用 C++17 和 Debug 构建，无需另外安装 ZeroMQ。
示例会自行启动服务端和客户端，使用 TCP 4444 端口，输出 `resp = 16` 和
`resp = 9` 后退出。若选择 ZMQ 通信层，则需额外安装 `libzmq3-dev`。


Compiling Test
=======

test/test.cpp is a simple test demonstrating how to use nanoRPC. You can compile it with `CMake`, `make`, or `Visual Studio`.

**Compiling With CMake**

To compile with `CMake`, use the following command:
```bash
user@myhost:~/projects/nanoRPC$ mkdir build
user@myhost:~/projects/nanoRPC$ cd build
user@myhost:~/projects/nanoRPC/build$ cmake .. -DCOMM_LAYER=ASIO
-- The C compiler identification is GNU 7.4.0
-- The CXX compiler identification is GNU 7.4.0
-- ...
-- Configuring done
-- Generating done
-- Build files have been written to: /home/user/projects/nanoRPC/build
user@myhost:~/projects/nanoRPC/build$ make
Scanning dependencies of target remmy_test
[ 50%] Building CXX object CMakeFiles/remmy_test.dir/test/test.cpp.o
[100%] Linking CXX executable remmy_test
[100%] Built target remmy_test
```

Note that we use `-DCOMM_LAYER=ASIO` option for `CMake` here. nanoRPC supports ASIO and ZMQ. Here we choose ASIO as the communication layer.

If you want to use ZeroMQ, you also need to install libzmq.

**Compiling With `make`**

There is a Makefile under `nanoRPC/test`, which can be used to compile the test. The communication can be switched by defining the `USE_ASIO` or `USE_ZMQ` at the top of the Makefile.

**Building With VS 2017/2019**

`remmy.sln` is a VS 2019 solution file. You can use VS 2019 to open the solution, select C++17 in the project settings, and compile the test project. However, if you choose to use VS 2017, you need to retarget the solution before you can build it. To retarget the solution, open it and right-click on the `Solution 'Remmy'` in `Solution Explorer`, then choose `Retarget Solution`. In the pop-up windows, choose `latest installed version` for `Windows SDK Version`.

By default the solution uses ASIO as the communication layer. Switching to ZeroMQ requires defining `USE_ZMQ` and `USE_ASIO` macros in project properties. You will also need to specify the location to ZeroMQ library when using ZeroMQ.


Programming interface
=======

```c++
// define protocol
class RPC_Protocol : public ProtocolWithUID<UniqueId("RPC_Proto")> {
public:
    ComplexType req;
    size_t resp;

    virtual void MarshallRequest(StreamBuffer & buf) {
        req.Serialize(buf);
    }

    virtual void MarshallResponse(StreamBuffer & buf) {
        Serialize(buf, resp);
    }

    virtual void UnmarshallRequest(StreamBuffer & buf) {
        req.Deserialize(buf);
    }

    virtual void UnmarshallResponse(StreamBuffer & buf) {
        Deserialize(buf, resp);
    }

    virtual void HandleRequest(void *server) {
        // as a demonstration, this protocol returns req.x + req.y + req.z.size(),
        // and adds this value to server, which is just a std::atomic<size_t>
        std::atomic<size_t>* s = static_cast<std::atomic<size_t>*>(server);
        resp = req.x + (size_t)req.y + req.z.size();
        // note that the handler can be executed by multiple threads at the same time,
        // we need to make it thread-safe
        s->fetch_add(resp);
        REMMY_WARN("Server is now %lu", s->load());
    }
};

    // start server
    // ...
    std::atomic<size_t> size = 0;
    rpc.RegisterProtocol<RPC_Protocol>(&size);
    rpc.StartServing();

    // now, create a client
    AsioEP ep(asio::ip::address::from_string("127.0.0.1"), port);
    RPC_Protocol proto;
    proto.req.x = 10;
    proto.req.y = 1.0;
    proto.req.z = "12345";
    ec = rpc.RpcCall(ep, proto);
    std::cout << "response = " << proto.resp << std::endl;
```

Please refer to [test/test.cpp](test/test.cpp) for an example on how to use nanoRPC.


Contributing
=======
Everyone is welcome to contribute to this project, either to improve the code or documentation.

The whole library can be divided into these parts:

* rpc_stub.h: contains RPCStub class, which is the entry point

* protocol.h: declares the interface of protocols

* serialize.h: implements the Serializer template class

* comm.h: declares the communication layer, which can be implemented with any network libray, such as asio (as in comm_asio.h), ZeroMQ, etc.

* other stuff: incluing logging, message structure, buffer, concurrent queue, unique id, ...

I will try to write as much document as possible in the files, but you are also welcome to contribute standalone document files.
