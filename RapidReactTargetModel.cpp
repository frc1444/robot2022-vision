#include "RapidReactTargetModel.h"

using namespace Lightning;

RapidReactTargetModel::RapidReactTargetModel() : TargetModel()
{
    _keyPoints = std::vector<cv::Point3d> {
        {0, 0, 0},
        {127, 0, 0},
        {0, 50.8, 0},
        {127, 50.8, 0},
    };

    _targetAxes = std::vector<cv::Point3d> {
        {0,0,0},
        {100,0,0},
        {0,100,0},
        {0,0,100}
    };
}

std::vector<cv::Point3d> RapidReactTargetModel::GetSubTargetKeyPoints(int subTarget) const 
{   
    // For 2022, there isn't really a sub target, so just return the full target keypoints
    return _keyPoints;
    
}