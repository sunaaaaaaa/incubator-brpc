#include  <gflags/gflags.h>
#include <gflags/gflags_declare.h>
#include <butil/logging.h>
#include <brpc/channel.h>
#include <brpc/parallel_channel.h>
#include <bthread/bthread.h>
#include <butil/time.h>
#include <bvar/bvar.h>
#include <sched.h>
#include <unistd.h>
#include <iostream>
#include <sys/time.h>
#include <iostream>
#include <vector>
#include "benchmark.pb.h"


DEFINE_string(attachment, "", "Carry this along with requests");
DEFINE_string(protocol, "baidu_std", "Protocol type. Defined in src/brpc/options.proto");
DEFINE_string(connection_type, "single", "Connection type. Available values: single, pooled, short");
DEFINE_string(server, "0.0.0.0:8000", "IP Address of server");
DEFINE_string(load_balancer, "", "The algorithm for load balancing");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)"); 
DEFINE_int32(interval_ms, 1000, "Milliseconds between consecutive requests");
DEFINE_bool(use_bthread, false, "Use bthread to send requests");

DEFINE_bool(rtt_count_test,true,"open RTT count test");
DEFINE_bool(rtt_time_test,true,"open RTT time test");
DEFINE_bool(qps_test,true,"open qps test");
DEFINE_int32(rtt_count,100000,"the rotal num of RTT test "); //RTT规定次数的测试总次数
DEFINE_uint32(rtt_time,10,"the total time of  rtt test"); //RTT规定时间的测试时长
DEFINE_int32(qps_parallecl,30,"the client channel parallel send request num");//并发请求线程数目


bvar::LatencyRecorder g_rtt_count_recorder("rtt_count_latency");
bvar::LatencyRecorder g_rtt_time_recorder("rtt_time_latency");
bvar::LatencyRecorder g_qps_recorder("qps_time_latency");
bvar::Adder<int> g_error_count("client_error_count");
namespace example{

std::string g_request = "hello world";
std::string g_attachment = "";

class  BenchMarkClient{
public:
        BenchMarkClient(){
             m_options.protocol = FLAGS_protocol;
             m_options.connection_type = FLAGS_connection_type;
             m_options.timeout_ms = FLAGS_timeout_ms;
             m_options.max_retry = FLAGS_max_retry;
        }
      
         void testRTTCountTest(){
                 std::cout << "start rtt count test,please wait a moment."<<std::endl;
                int32_t count =  FLAGS_rtt_count;
                brpc::Channel channel;
                if(channel.Init(FLAGS_server.c_str(),FLAGS_load_balancer.c_str(),&m_options)!=0){
                      LOG(ERROR) << "init channel error";
                      return ;
                }

                example::BenchMarkService_Stub stub(&channel);
                int32_t i = 0;
                int32_t log_id = 0;
                while( i < count){
                    i++;
                    example::TestRequest  request;
                    example::TestResponse  response;
                    brpc::Controller  cntl;

                    request.set_message("hello world");
                    cntl.set_log_id(log_id);
                    cntl.request_attachment().append(FLAGS_attachment);
                    stub.test(&cntl,&request,&response,NULL);
                    if(!cntl.Failed()){
                        g_rtt_count_recorder << cntl.latency_us();
                    }else{
                        LOG(WARNING)<<cntl.ErrorText();
                    }
                    ++log_id;
                }

                std::cout << "RTT Time test:"<<std::endl;
                 std::cout<< "avg rtt: " << g_rtt_count_recorder.latency() <<std::endl; 
                 std::cout<< "max rtt: " <<g_rtt_count_recorder.max_latency() <<std::endl; 
                 std::cout<< "50% rtt: " << g_rtt_count_recorder.latency_percentiles()[0]<<std::endl; 
                 std::cout<< "90% rtt: " << g_rtt_count_recorder.latency_percentiles()[1]<<std::endl; 
                 std::cout<< "99% rtt: " << g_rtt_count_recorder.latency_percentiles()[2]<<std::endl; 
                 std::cout<< "99.9% rtt: " << g_rtt_count_recorder.latency_percentiles()[3]<<std::endl; 
        }

        void testRTTTImeTest(){
                std::cout << "start rtt time test,please wait a moment."<<std::endl;
                uint32_t  time = FLAGS_rtt_time;
                brpc::Channel channel;
                if(channel.Init(FLAGS_server.c_str(),FLAGS_load_balancer.c_str(),&m_options)!=0){
                      LOG(ERROR) << "init channel error";
                      return ;
                }
                example::BenchMarkService_Stub stub(&channel);
                int log_id = 0;
                
                timeval nowTime;
                gettimeofday(&nowTime,NULL);//获取当前的us
                timeval  endTime;
                 gettimeofday(&endTime,NULL);
                 endTime.tv_sec = nowTime.tv_sec + time;
                 time_t end =  endTime.tv_sec * 1000000 + endTime.tv_usec;
                 while((nowTime .tv_sec *1000000 + nowTime.tv_usec) < end){
                            example::TestRequest  request;
                            example::TestResponse  response;
                            brpc::Controller  cntl;
                            request.set_message("hello world");
                            cntl.set_log_id(log_id);
                            cntl.request_attachment().append(FLAGS_attachment);
                            stub.test(&cntl,&request,&response,NULL);
                            if(!cntl.Failed()){
                                g_rtt_time_recorder << cntl.latency_us();
                            }else{
                                LOG(WARNING)<<cntl.ErrorText();
                            }
                            gettimeofday(&nowTime,NULL);
                            ++log_id;
                 }
                 std::cout << "RTT Time test:"<<std::endl;
                 std::cout<< "avg rtt: " << g_rtt_time_recorder.latency() <<std::endl; 
                 std::cout<< "max rtt: " <<g_rtt_time_recorder.max_latency() <<std::endl; 
                 std::cout<< "50% rtt: " << g_rtt_time_recorder.latency_percentiles()[0]<<std::endl; 
                 std::cout<< "90% rtt: " << g_rtt_time_recorder.latency_percentiles()[1]<<std::endl; 
                 std::cout<< "99% rtt: " << g_rtt_time_recorder.latency_percentiles()[2]<<std::endl; 
                 std::cout<< "99.9% rtt: " << g_rtt_time_recorder.latency_percentiles()[3]<<std::endl; 
                 //g_rtt_time_recorder.
        }
        void testQPSTest(){
                   std::cout << "start qps test,please wait a moment."<<std::endl;
                    //int32_t  threadNum = FLAGS_qps_parallecl;
                    brpc::ParallelChannel  channel;
                    brpc::ParallelChannelOptions  pchan_options;
                    pchan_options.timeout_ms = FLAGS_timeout_ms;

                    if(channel.Init(&pchan_options)!=0){
                        LOG(ERROR) << "error init parallel channel";
                        return ;
                    }

                    for(int i = 0;i<FLAGS_qps_parallecl;++i){
                            brpc::Channel* sub_channel = new brpc::Channel;

                            if(sub_channel->Init(FLAGS_server.c_str(),FLAGS_load_balancer.c_str(),&m_options)!=0){
                                LOG(ERROR) << "intit channel error";
                                return;
                            }
                            if(channel.AddChannel(sub_channel,brpc::OWNS_CHANNEL,NULL,NULL)!=0){
                                LOG(ERROR) << "error to add channel,i = " << i;
                                return;
                            }
                    }
                    
                    //g_qps_recorder = new bvar::LatencyRecorder[FLAGS_qps_parallecl];
                    // for(int i = 0;i < FLAGS_qps_parallecl ;++i){
                         
                    // }

                    std::vector<bthread_t> bids;
                    std::vector<pthread_t> pids;

                    if(FLAGS_use_bthread){
                           bids.resize(FLAGS_qps_parallecl);
                           for(int i = 0;i<FLAGS_qps_parallecl;++i){
                                   if(bthread_start_background(&bids[i],NULL,qpsTest,&channel)!=0){
                                        LOG(ERROR) << "Fail to create bthread";
                                        return ;
                                   } 
                           }
                              
                    }else{
                            pids.resize(FLAGS_qps_parallecl);

                            for(int i = 0;i<FLAGS_qps_parallecl;++i){
                                   if(pthread_create(&pids[i],NULL,qpsTest,&channel)!=0){
                                            LOG(ERROR) << "Fail to create pthread";
                                            return ;
                                   } 
                            }
                           
                    }
                    
                     while (!brpc::IsAskedToQuit()) {
                            sleep(1);
                            LOG(INFO) << "Sending EchoRequest at qps=" << g_qps_recorder.qps(1)
                                    << " latency=" << g_qps_recorder.latency(1) << noflush;
                            LOG(INFO);
                        }
                        
                    std::cout << "QPS Time test:"<<std::endl;
                    std::cout<< "avg rtt: " << g_qps_recorder.latency() <<std::endl; 
                    std::cout<< "max rtt: " <<g_qps_recorder.max_latency() <<std::endl; 
                    std::cout<< "50% rtt: " << g_qps_recorder.latency_percentiles()[0]<<std::endl; 
                    std::cout<< "90% rtt: " << g_qps_recorder.latency_percentiles()[1]<<std::endl; 
                    std::cout<< "99% rtt: " << g_qps_recorder.latency_percentiles()[2]<<std::endl; 
                    std::cout<< "99.9% rtt: " << g_qps_recorder.latency_percentiles()[3]<<std::endl; 


                    if(FLAGS_use_bthread){
                           for(int i = 0;i<FLAGS_qps_parallecl;++i){
                                   bthread_join(bids[i],NULL); 
                            } 
                     } else{
                             for(int i = 0;i<FLAGS_qps_parallecl;++i){
                                 std::cout << i <<std::endl;
                                 pthread_join(pids[i],NULL);
                           } 
                     } 
        }


        static  void*  qpsTest(void* arg){
                  example::BenchMarkService_Stub  stub(static_cast<google::protobuf::RpcChannel*>(arg));
                  int log_id = 0;

                  while(!brpc::IsAskedToQuit()){
                        example::TestRequest request;
                        example::TestResponse response;
                        brpc::Controller cntl;

                         request.set_message(g_request);
                         cntl.set_log_id(log_id++);  // set by user
                        // Set attachment which is wired to network directly instead of 
                        // being serialized into protobuf messages.
                        cntl.request_attachment().append(g_attachment);

                        // Because `done'(last parameter) is NULL, this function waits until
                        // the response comes back or error occurs(including timedout).
                        stub.test(&cntl, &request, &response, NULL);
                         if (!cntl.Failed()) {
                               g_qps_recorder << cntl.latency_us();
                       } else {
                                g_error_count << 1; 
                                LOG(ERROR) << "error=" << cntl.ErrorText() << " latency=" << cntl.latency_us();
                                //bthread_usleep(50000);
                      }
                  }

                  return NULL;
        }
 private:
        brpc::ChannelOptions m_options;      
};
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
    GFLAGS_NS::ParseCommandLineFlags(&argc,&argv,true);
     
     example::BenchMarkClient client;
     if(FLAGS_rtt_count_test){
             client.testRTTCountTest();
     }
     
    if(FLAGS_rtt_time_test){
            client.testRTTTImeTest();
    } 
     
    if(FLAGS_qps_test){
         client.testQPSTest();
    } 
    return 0;
}