#include "RapidReactTargetModel.h"

using namespace Lightning;

RapidReactTargetModel::RapidReactTargetModel() : TargetModel()
{
    _keyPoints = std::vector<cv::Point3d> {
        {0, 0, -500},
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
    std::vector<cv::Point3d> keyPoints{ _keyPoints[1], _keyPoints[2], _keyPoints[3], _keyPoints[4] }; 
    return keyPoints;
    
}