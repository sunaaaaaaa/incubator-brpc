#include  <gflags/gflags.h>
#include <gflags/gflags_declare.h>
#include <butil/logging.h>
#include <brpc/server.h>
#include <sched.h>
#include <unistd.h>
#include <iostream>
#include "benchmark.pb.h"

DEFINE_bool(test_attachment,true,"attachment data use to change message length");
DEFINE_int32(port,8000,"TCP Port of this server");
DEFINE_string(listen_addr,"","Server listen address, may be IPV4/IPV6/UDS."
            " If this is set, the flag port will be ignored");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state "
             "(waiting for client to close connection before server stops)");
DEFINE_int32(max_concurrency, 0, "Limit of request processing in parallel");  //规定在途请求数目
DEFINE_int32(thread_num, 10, "thread nums for service");  
//压力测试  服务端和Echo服务器逻辑一致，只需要将消息原样返回即可
 namespace example{
 class BenchMarkServiceImpl:public BenchMarkService{
public:
        BenchMarkServiceImpl(){}
         ~BenchMarkServiceImpl(){}
         virtual void  test(::google::protobuf::RpcController* controller,
                       const ::example::TestRequest* request,
                       ::example::TestResponse* response,
                       ::google::protobuf::Closure* done){
                //确保同步rpc的情况下，done一定会被调用
                brpc::ClosureGuard done_guard(done);

                brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);

                LOG(INFO) << "Received request[log_id = " << cntl->log_id()
                                       << "] from " << cntl->remote_side()
                                        << " to " << cntl->local_side()
                                        << " : " << request->message()
                                        <<"(attached=" << cntl->request_attachment() << ")";
                
                response->set_message(request->message());


                if(FLAGS_test_attachment){
                     cntl->response_attachment().append(cntl->request_attachment());
                }                                          
         }
 } ;   
 } 

int main(int argc,char* argv[]){
    
    //服务端进程绑定于一个CPU核心
    cpu_set_t   cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1,&cpuset);
  
    if(sched_setaffinity(0,sizeof(cpuset),&cpuset) == -1){
           std::cout << "bind process to fix cpu core failed " << std::endl;
    }

    //解析参数
    google::ParseCommandLineFlags(&argc,&argv,true);

     brpc::Server server;
     example::BenchMarkServiceImpl benchMarkService;

     if(server.AddService(&benchMarkService,brpc::SERVER_DOESNT_OWN_SERVICE) != 0){
         LOG(ERROR) << "add service error";
         return -1;
     }
     
     butil::EndPoint endpoint;
     if(!FLAGS_listen_addr.empty()){
          if(butil::str2endpoint(FLAGS_listen_addr.c_str(),&endpoint) < 0){
               LOG(ERROR) << "Invalid listen address:" << FLAGS_listen_addr;
                return -1;
          }
     }else{
         endpoint = butil::EndPoint(butil::IP_ANY,FLAGS_port);
     } 
    bthread_setconcurrency(FLAGS_thread_num); 
    brpc::ServerOptions options;
    options.idle_timeout_sec = FLAGS_idle_timeout_s;
    options.num_threads =  FLAGS_thread_num;
    if(server.Start(endpoint,&options)!=0){
        LOG(ERROR) << "start server error";
        return -1;
    }
    server.RunUntilAskedToQuit();
    return 0;
}