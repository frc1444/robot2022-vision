#include "RapidReactTargetModel.h"

using namespace Lightning;

RapidReactTargetModel::RapidReactTargetModel() : TargetModel()
{
    _keyPoints = std::vector<cv::Point3d> {
        {498.475, 0, 0},
        {0, 0, 0},
        {58.6486, 0, 0},
        {938.3014, 0, 0},
        {996.95, 0, 0},
        {278.384, 381, 0},
        {718.566, 381, 0},
        {249.047, 431.8, 0},
        {747.903, 431.8, 0}  
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
    // TODO this could be better - may require redesign of target model

    std::vector<cv::Point3d> keyPoints;

    if (subTarget == 0)
    {
        // For 2020 this returns the boundingbox corners of the target + center
        keyPoints = std::vector<cv::Point3d> {
           // {0, 0, 0},
            {-498.475, 0, 0},
            {498.475, 0, 0},
            {-249.428, 431.8, 0},
            {249.428, 431.8, 0} 
        };
    }

    return keyPoints;
    
}