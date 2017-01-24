#ifndef EC2DNS_EC2DNSCLIENT_H
#define EC2DNS_EC2DNSCLIENT_H

#include "CloudDnsClient.h"

#include "aws/autoscaling/AutoScalingClient.h"
#include "aws/autoscaling/model/DescribeAutoScalingGroupsRequest.h"
#include "aws/ec2/EC2Client.h"
#include "aws/ec2/model/DescribeInstancesRequest.h"

using namespace Aws::AutoScaling;
using namespace Aws::EC2;


class Ec2DnsClient : public CloudDnsClient {
public:
    Ec2DnsClient(
        const std::shared_ptr<EC2Client> ec2Client,
        const std::shared_ptr<AutoScalingClient> asgClient,
        const CloudDnsConfig config,
        std::shared_ptr<StatsReceiver> statsReceiver
    ) : CloudDnsClient(config, statsReceiver),
        m_ec2Client(ec2Client), m_asgClient(asgClient)
    {
    }

    static std::shared_ptr<CloudDnsClient> Create(CloudDnsConfig &dnsConfig, std::shared_ptr<StatsReceiver> statsReceiver);

protected:
    virtual bool _DescribeInstances(const std::string& instanceId, const std::string& ip, std::vector<Instance> *instances);
    virtual bool _DescribeAutoscalingGroups(std::unordered_map<std::string, const std::unordered_set<std::string>> *results);

private:
    template<class TRequest, class TResponse, class TError>
    bool _CallApi(
        std::string apiTag,
        TRequest& request,
        std::function<Aws::Utils::Outcome<TResponse, Aws::Client::AWSError<TError>>(const TRequest&)> requestFn,
    std::vector<TResponse> *responses) {
      std::string nextToken;
      do {
        this->m_apiRequests->Increment();
        auto ret = requestFn(request);
        LOG(INFO) << "API Request complete";
        if (!ret.IsSuccess()) {
          this->m_apiFailures->Increment();
          auto errorMessage = ret.GetError().GetMessage();
          LOG(ERROR) << "API request " << apiTag << " failed with error: " << errorMessage;
          return false;
        }
        this->m_apiSuccesses->Increment();

        auto result = ret.GetResult();
        responses->push_back(result);
        nextToken = result.GetNextToken();
        request.SetNextToken(nextToken);
      } while (!nextToken.empty());
      return true;
    };

    std::shared_ptr<EC2Client> m_ec2Client;
    std::shared_ptr<AutoScalingClient> m_asgClient;

};


#endif //EC2DNS_EC2DNSCLIENT_H
