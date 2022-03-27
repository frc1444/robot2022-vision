#pragma once

#include <opencv2/opencv.hpp>

#include "TargetModel.h"

namespace Lightning
{
class RapidReactTargetModel : public TargetModel
{
public:

    RapidReactTargetModel();

    virtual std::vector<cv::Point3d> GetSubTargetKeyPoints(int) const;
};
}