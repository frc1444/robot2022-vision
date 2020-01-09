#pragma once

#include <opencv2/opencv.hpp>

#include "TargetModel.h"

namespace Lightning
{
class InfiniteRechargeTargetModel : public TargetModel
{
public:

    InfiniteRechargeTargetModel();

    virtual std::vector<cv::Point3d> GetSubTargetKeyPoints(int) const;
};
}