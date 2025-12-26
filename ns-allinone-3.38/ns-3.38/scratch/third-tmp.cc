// 短期使用，用于测试各组件功能
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <time.h>

#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"
// 确保之前的任何关于 PGO_TRAINING 的设置都被清除
// 设定一个新的配置文件路径

#include "ns3/core-module.h"
#include "ns3/qbb-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet.h"
#include "ns3/error-model.h"
#include "ns3/rdma.h"
#include "ns3/rdma-client.h"
#include "ns3/rdma-client-helper.h"
#include "ns3/rdma-driver.h"
#include "ns3/switch-node.h"
#include "ns3/sim-setting.h"

using namespace ns3;
using namespace std;

int main(int argc, char *argv[])
{
    // 打印所有已注册的 TypeId
    std::cout << "=== Registered TypeIds ===" << std::endl;
    for (uint32_t i = 0; i < TypeId::GetRegisteredN(); ++i) {
        TypeId tid = TypeId::GetRegistered(i);
        std::cout << tid.GetName() << std::endl;
    }

    // 安全查找 DropTailQueue<Packet>
    std::string queueName = "ns3::DropTailQueue<Packet>";
    TypeId tid;

    bool found = TypeId::LookupByNameFailSafe(queueName, &tid);

    if (!found || tid.GetUid() == 0) {
        std::cerr << "TypeId not found: " << queueName << std::endl;
    } else {
        std::cout << "Found TypeId: " << tid.GetName()
                  << " [Uid=" << tid.GetUid() << "]" << std::endl;
    }

    // 可选：尝试其他可能的名称（以防模板命名差异）
    std::vector<std::string> alternatives = {
        "ns3::DropTailQueue<ns3::Packet>",
        "ns3::Queue",
        "ns3::QueueBase"
    };

    for (const auto& alt : alternatives) {
        TypeId altTid;
        if (TypeId::LookupByNameFailSafe(alt, &altTid)) {
            std::cout << "Alternative found: " << altTid.GetName() 
                      << " [Uid=" << altTid.GetUid() << "]" << std::endl;
        }
    }

    return 0;
}