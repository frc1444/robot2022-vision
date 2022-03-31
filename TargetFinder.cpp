#include "TargetFinder.h"
#include "CameraModel.h"
#include "TargetModel.h"
#include "Setup.h"
#include "VisionData.hpp"

using namespace Lightning;

TargetFinder::TargetFinder(std::vector<spdlog::sink_ptr> sinks, std::string name, std::unique_ptr<TargetModel> targetModel, std::unique_ptr<CameraModel> cameraModel, cv::Vec3d offsets)
    : _targetModel(std::move(targetModel))
    , _cameraModel(std::move(cameraModel))
    , _name(name)
    , _offset(offsets)
{
    _logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    _logger->set_level(Lightning::Setup::Diagnostics::LogLevel);
}

bool TargetFinder::Process(cv::Mat& image, std::vector<VisionData>& data)
{
    // Convert image to HSV and gray
    cv::Mat hsvImage, grayImage;
    ConvertImage(image, hsvImage, grayImage);

    // Filter based on color
    cv::Mat rangedImage;

    FilterOnColor(hsvImage, rangedImage, cv::Scalar(Setup::HSVFilter::LowH, Setup::HSVFilter::LowS, Setup::HSVFilter::LowV), cv::Scalar(Setup::HSVFilter::HighH, Setup::HSVFilter::HighS, Setup::HSVFilter::HighV), Setup::HSVFilter::MorphologyIterations);

    // Detect contours
    std::vector<std::vector<cv::Point>> contours;

    if (!FindContours(rangedImage, contours))
    {
        // No contours, so nothing to process
        return false;
    }

    // Approximate contours
    std::vector<std::vector<cv::Point>> approx(contours.size());
    cv::Mat contourImage = cv::Mat(image.size(), CV_8UC1);

    ApproximateContours(contours, approx, contourImage);

    // Find target sections
    std::vector<TargetSection> targetSections;

    TargetSectionsFromContours(approx, targetSections, cv::Size(image.cols, image.rows));
    //TargetSectionsFromContours(contours, targetSections, cv::Size(image.cols, image.rows));

    // Create targets from sections
    std::vector<Target> targets;

    SortTargetSections(targetSections, targets);

    // Get subpixel measurement on target corners
    RefineTargetCorners(targets, grayImage);

    // Find the camera to target tranform
    FindTargetTransforms(targets, cv::Size(image.cols, image.rows));

    // Sort targets by horizontal position in image
    std::sort(targets.begin(), targets.end(), [](Target t1, Target t2){ return (t1.data.imageX < t2.data.imageX); });

    // Add targets to data packet - TODO this could be cleaner - redo VisionPacket?
    for (int i = 0; i < (int)targets.size(); ++i)
    {
        targets[i].data.targetId = i;

        data.push_back(targets[i].data);
    }

    if (Setup::Diagnostics::DisplayDebugImages)
    {
        DrawDebugImage(image, targets);

        _debugImages.clear();
        _debugImages.push_back(std::make_pair("Raw", image));
        _debugImages.push_back(std::make_pair("Contours", contourImage));
        _debugImages.push_back(std::make_pair("Ranged", rangedImage));
    }

    return true;
}

void TargetFinder::ConvertImage(const cv::Mat& image, cv::Mat& hsv, cv::Mat& gray)
{
    // Convert image to HSV and gray
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);

    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
}

void TargetFinder::FilterOnColor(const cv::Mat& hsv, cv::Mat& ranged, const cv::Scalar low, const cv::Scalar high, const int iter)
{
    // Filter based on color
    cv::inRange(hsv, low, high, ranged);

    // Close disconnected contours
    cv::morphologyEx(ranged, ranged, cv::MORPH_CLOSE, cv::Mat(), cv::Point(-1,-1), iter);

    // Blur?
}

bool TargetFinder::FindContours(const cv::Mat& image, std::vector<std::vector<cv::Point>>& contours)
{
    // Find contours
    std::vector<cv::Vec4i> hierarchy;

    cv::findContours(image, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Filter out small contours
    std::vector<std::vector<cv::Point>>::iterator itc = contours.begin();

    while (itc != contours.end())
    {
        if (itc->size() < Setup::Processing::ContourSizeThreshold)
        {
            itc = contours.erase(itc);
        }
        else
        {
            ++itc;
        }
    }

    if (contours.size() <= 0)
    {
        _logger->debug("FindContours(): No contours found 1.");
        return false;
    }

    return true;
}

void TargetFinder::ApproximateContours(const std::vector<std::vector<cv::Point>>& contours, std::vector<std::vector<cv::Point>>& approximation, cv::Mat& image)
{

    if (Setup::Diagnostics::DisplayDebugImages)
    {
        image = cv::Mat::zeros(image.size(), CV_8UC1);
    }

    // Approximate
    approximation.resize(contours.size());

    for (int i = 0; i < contours.size(); ++i)
    {
        std::vector<cv::Point> hull;

        cv::convexHull(contours[i], hull);

        cv::approxPolyDP(hull, approximation[i], Setup::Processing::ContourApproximationAccuracy, true);

        _logger->trace("Contour {0} approxPoly Points: {1}", i, approximation[i].size());

        if (Setup::Diagnostics::DisplayDebugImages)
        {
            cv::drawContours(image, approximation, i, cv::Scalar(255), cv::FILLED, cv::LINE_AA);
        }
    }

}

void TargetFinder::TargetSectionsFromContours(const std::vector<std::vector<cv::Point>>& contours, std::vector<TargetSection>& sections, const cv::Size size)
{
    sections.clear();

    double sum = 0.0;
    double sqsum = 0.0;
    for (int i = 0; i < (int)contours.size(); ++i)
    {
        auto rect = cv::minAreaRect(contours[i]);
        sum += rect.size.area();
        sqsum += std::pow(rect.size.area(), 2);
    }

    double averageArea = sum /= contours.size();
    double stDev = std::sqrt( sqsum / contours.size() - std::pow(averageArea, 2));

    _logger->trace("Average Contour Area {0}",averageArea);
    _logger->trace("Contour Area StDev {0}",stDev);

    // Evaluate each contour to see if it is a target section
    for (int i = 0; i < (int)contours.size(); ++i)
    {       
        _logger->trace("Contour {0} Size {1}", i, contours[i].size());

        if (contours[i].size() != 4)
        {
            continue;
        }

        double area = cv::contourArea(contours[i], false);  
        double perimeter = cv::arcLength(contours[i], true);

        double shapeFactor = (4 * CV_PI * area) / std::pow(perimeter, 2);

        _logger->trace("Contour {0} Shape Factor {1}", i, shapeFactor);

        if (shapeFactor > Setup::Processing::ShapeFactorMin && shapeFactor < Setup::Processing::ShapeFactorMax)
        {
            // Get bounding box and angle
            auto rect = cv::minAreaRect(contours[i]);

            if (rect.size.area() < (averageArea - 1.25 * stDev) || rect.size.area() > (averageArea +  1.25 * stDev))
            {
                _logger->trace("Area outlier {0} {1}", rect.size.area(), averageArea);
                continue;
            }

            // Reject contour if it is too close to the edge of the image

            double imageEdgeThreshold = Setup::Processing::ImageEdgeThreshold;
            
            if (rect.center.x < imageEdgeThreshold || 
                rect.center.x > Setup::Camera::Width - imageEdgeThreshold || 
                rect.center.y < imageEdgeThreshold ||
                rect.center.y > Setup::Camera::Height - imageEdgeThreshold)
            {
                continue;
            }

            // Save corner points
            std::vector<cv::Point2f> points(4);

            cv::Point2f rect_points[4];
            rect.points(rect_points);

            cv::Point2f center(0,0);
            for (int j = 0; j < contours[i].size(); ++j)
            {
                points[j] = cv::Point2f(contours[i][j].x, contours[i][j].y);
                center += points[j];
            }

            center /= 4;
            

            for (int j = 0; j < 4; ++j)
            {
                //points[j] = rect_points[j];
            }

            if (rect.size.width < rect.size.height)
            {
                rect.angle += 180;
            }
            else
            {
                rect.angle += 90;
            }
            

            TargetSection section { points, rect, shapeFactor, center, area };

            sections.push_back(section);
        }
    }
}

void TargetFinder::SortTargetSections(const std::vector<TargetSection>& sections, std::vector<Target>& targets)
{
    targets.clear();

    for (int i = 0; i < (int)sections.size(); ++i)
    {
        Target newTarget;
        newTarget.sections.push_back(sections[i]);
        targets.push_back(newTarget);
    }
}

void TargetFinder::RefineTargetCorners(std::vector<Target>& targets, const cv::Mat& image)
{
    for (auto& target : targets)
    {   
        target.center = cv::Point2f(0,0);

        // Get sub pixels for each corner
        for (auto& section : target.sections)
        {
            if (section.corners.size() <= 0)
            {
                continue;
            }

            try
            {
                cv::cornerSubPix(image, section.corners, cv::Size(5,5), cv::Size(-1,-1), cv::TermCriteria(cv::TermCriteria::MAX_ITER | cv::TermCriteria::EPS, Setup::Processing::MaxCornerSubPixelIterations, Setup::Processing::CornerSubPixelThreshold));
            }
            catch (cv::Exception ex)
            {
                _logger->error("RefineTargetCorners() caught exception: {0}", ex.what());
                continue;
            }

            std::vector<cv::Point2f> corners(4);

            for (int j = 0; j < 4; ++j)
            {
                if (section.corners[j].x < section.center.x)
                {
                    if (section.corners[j].y < section.center.y)
                    {
                        corners[1] = section.corners[j];
                    }
                    else
                    {                        
                        corners[3] = section.corners[j];
                    }
                }
                else
                {
                    if (section.corners[j].y < section.center.y)
                    {
                        corners[0] = section.corners[j];
                    }
                    else
                    {
                        corners[2] = section.corners[j];
                    }
                }
            }

            section.corners = corners;

            target.center += section.corners[0];
            target.center += section.corners[1];

        }

        target.center /= 2;
    }
}

void TargetFinder::FindTargetTransforms(std::vector<Target>& targets, const cv::Size& imageSize)
{
    // Get model key points
    std::vector<cv::Point3d> keyPoints;

    for (auto& target : targets)
    {
        // Set image points
        std::vector<cv::Point2d> imagePoints;

        if (target.sections.size() == 1)
        {
            keyPoints = _targetModel->GetSubTargetKeyPoints(0);

            imagePoints = std::vector<cv::Point2d>
            {
                //target.center,
                target.sections[0].corners[0],
                target.sections[0].corners[1],
                target.sections[0].corners[2],
                target.sections[0].corners[3]
            };
        }
        else
        {
            _logger->error("Incorrect number of target sections: {0}", target.sections.size());
            continue;
        }

        cv::Mat rvec = cv::Mat::zeros(3, 1, CV_64FC1);     
        cv::Mat tvec = cv::Mat::zeros(3, 1, CV_64FC1);

        tvec.at<double>(0,0) = 0;
        tvec.at<double>(1,0) = 0;
        tvec.at<double>(2,0) = 2500;

        rvec.at<double>(0,0) = -0.3;
        rvec.at<double>(1,0) = 0;
        rvec.at<double>(2,0) = 0;

        if (keyPoints.size() <= 0 || imagePoints.size() <= 0)
        {
            _logger->error("KeyPoints and ImagePoints can not be empty. {0}, {1}", keyPoints.size(), imagePoints.size());
            continue;
        }

        // Find transform
        bool solved = cv::solvePnP(keyPoints, imagePoints, _cameraModel->GetCameraMatrix(), _cameraModel->GetDistanceCoefficients(), rvec, tvec, true, cv::SOLVEPNP_AP3P);

        if (!solved)
        {
            _logger->debug("Failed to find target transform");  // TODO target id?
            target.data.status = VisionStatus::ProcessingError;
            continue;
        }

        // Apply robot-to-camera offsets while solution is still in robot coordinates
        tvec.at<double>(0,0) -= _offset[0];
        tvec.at<double>(1,0) -= _offset[1];
        tvec.at<double>(2,0) -= _offset[2];

        // Convert rotation vector to rotation matrix
        cv::Mat R;
        cv::Rodrigues(rvec, R);

        // Compute inverse of transform - this gives camera position in target coordinates
        if (Setup::Processing::UseWorldCoordinates)
        {
            R = R.t();              // transpose of R which is also the inverse
            tvec = -R * tvec;       // inverse of tvec

            cv::Rodrigues(R, rvec);
        }
        
        // Build transform matrix - not used currently
        /*
        cv::Mat P = cv::Mat::eye(4, 4, R.type());           // T is 4x4
        P( cv::Range(0,3), cv::Range(0,3) ) = R * 1;        // copies R into T
        P( cv::Range(0,3), cv::Range(3,4) ) = tvec * 1;     // copies tvec into T
        */

        // Get Euler angles from rotation maxtrix
        cv::Vec3d euler = EulerAnglesFromRotationMaxtrix(R);

        euler[0] *= (180/CV_PI);
        euler[1] *= (180/CV_PI);
        euler[2] *= (180/CV_PI);

        // Offset the target center to the true center of the target - the above solution is "centered" on the left-most point of the target (i.e. x = 0)
        cv::Point3d centerOffset(keyPoints[0].x, keyPoints[0].y, keyPoints[0].z);

        target.data.status = VisionStatus::TargetFound;

        target.data.x = tvec.at<double>(0,0);// - centerOffset.x;   // TODO
        target.data.y = tvec.at<double>(1,0);// - centerOffset.y;
        target.data.z = tvec.at<double>(2,0);//- centerOffset.z;
        target.data.pitch = euler[0];
        target.data.yaw = euler[1];
        target.data.roll = euler[2];
        target.data.imageX = (target.center.x - (imageSize.width / 2.0)) / (imageSize.width / 2.0);
        target.data.imageY = ((imageSize.height / 2.0) - target.center.y) / (imageSize.height / 2.0);
        target.rvec = rvec;
        target.tvec = tvec;

        // Account for camera offset from shooter
        target.data.x += 215;

        double theta = (180/ CV_PI) * atan2(target.data.x, target.data.z);
        target.data.theta = -theta;

        if (target.data.z < 500)
        {
            target.data.status = VisionStatus::NoTargetFound;
            _logger->trace("Invalid z found");
        }

        /*
        if (target.data.x > 0)
        {
            target.theta = 180 - theta;
        } 
        else
        {
            target.theta = -(theta + 180);
        }
        */
        target.robotDistance = (std::sqrt(std::pow(target.data.x, 2) + std::pow(target.data.z, 2)));
    }

}

double TargetFinder::Distance(const cv::Point2d& pt1, const cv::Point2d& pt2)
{
    double dX(pt1.x - pt2.x);
    double dY(pt1.y - pt2.y);

    return std::sqrt(std::pow(dX, 2) + std::pow(dY, 2));
}

void TargetFinder::ShowDebugImages()
{
    for (auto& image : _debugImages)
    {
        auto windowName = fmt::format("{0} {1}",_name, image.first);

        cv::namedWindow(windowName, cv::WINDOW_KEEPRATIO);
        cv::imshow(windowName, image.second);
    }
}

cv::Vec3d TargetFinder::EulerAnglesFromRotationMaxtrix(const cv::Mat& R)
{     
    double sy = std::sqrt(R.at<double>(0,0) * R.at<double>(0,0) +  R.at<double>(1,0) * R.at<double>(1,0) );
 
    cv::Vec3d vec;
    if (sy > 1e-6)  // Is the matrix singular
    {
        vec[0] = atan2(R.at<double>(2,1) , R.at<double>(2,2));  // x
        vec[1] = atan2(-R.at<double>(2,0), sy);                 // y
        vec[2] = atan2(R.at<double>(1,0), R.at<double>(0,0));   // z
    }
    else
    {
        vec[0] = atan2(-R.at<double>(1,2), R.at<double>(1,1));  // x
        vec[1] = atan2(-R.at<double>(2,0), sy);                 // y
        vec[2] = 0;                                             // z
    }

    return vec;  
}

void TargetFinder::DrawDebugImage(cv::Mat& image, const std::vector<Target>& targets)
{
    cv::RNG rng(4343466);

    double averageTheta = 0;
    int targetCount = 0;

    for (int target = 0; target < (int)targets.size(); ++target)
    {            
        if (targets[target].data.status != VisionStatus::TargetFound)
        {
            continue;
        }

        averageTheta += targets[target].data.theta;
        targetCount++;

        cv::Scalar color(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));

        // Project target points back onto image
        cv::Mat rvec = targets[target].rvec;
        cv::Mat tvec = targets[target].tvec;

        // Undo offsets camera-to-robot offsets so image is drawn correctly
        tvec.at<double>(0,0) += _offset[0];
        tvec.at<double>(1,0) += _offset[1];
        tvec.at<double>(2,0) += _offset[2];


        if (Setup::Processing::UseWorldCoordinates)
        {
            targets[target].GetInverseTransforms(rvec, tvec);
        }
        
        std::vector<cv::Point2d> projectedPoints;
        //cv::projectPoints(_targetModel->GetSubTargetKeyPoints(0), rvec, tvec, _cameraModel->GetCameraMatrix(), _cameraModel->GetDistanceCoefficients(), projectedPoints);       
        cv::projectPoints(_targetModel->GetKeyPoints(), rvec, tvec, _cameraModel->GetCameraMatrix(), _cameraModel->GetDistanceCoefficients(), projectedPoints);       

        //cv::circle(image, targets[target].center, 5, color, 1, cv::LINE_AA);
        
        for (auto& pt : targets[target].sections[0].corners)
        {
           // cv::circle(image, pt, 5, color, 1, cv::LINE_AA);
        }      
        
        for (int j(1); j < 2; ++j)
        {
            cv::circle(image, targets[target].sections[0].corners[j], 5, color, 1, cv::LINE_AA);
        }     
        
        for (auto& pt : projectedPoints)
        {
            cv::circle(image, pt, 1, color, cv::FILLED, cv::LINE_AA);
        }

        // Show target section bounding boxes
        for (auto section : targets[target].sections)
        { 
            // TODO DO THIS BETTER AND SOMEWHERE ELSE
            if (section.rect.size.width < section.rect.size.height)
            {
                section.rect.angle -= 180;
            }
            else
            {
                section.rect.angle -= 90;
            }

            cv::Point2f vertices[4]; 
            section.rect.points(vertices);

            for( int j = 0; j < 4; j++ )
            {
                //cv::line( image, vertices[j], vertices[(j+1)%4], color, 1, cv::LINE_AA );
            }
        }

        // Show target data
        std::vector<std::string> imageText;   

        // TODO make this into a function?
        imageText.push_back(fmt::format("X: {:03.1f}", targets[target].data.x / 25.4));
        imageText.push_back(fmt::format("Y: {:03.1f}", targets[target].data.y / 25.4));
        imageText.push_back(fmt::format("Z: {:03.1f}", targets[target].data.z / 25.4));

        //imageText.push_back(fmt::format("Roll: {:03.1f}", targets[target].data.roll));
        //imageText.push_back(fmt::format("Pitch: {:03.1f}", targets[target].data.pitch));
        //imageText.push_back(fmt::format("Yaw: {:03.1f}", targets[target].data.yaw));

        imageText.push_back(fmt::format("Theta: {:03.1f}", targets[target].data.theta));
        imageText.push_back(fmt::format("rDist: {:03.1f}", targets[target].robotDistance / 25.4));

        for (int i = 0; i < (int)imageText.size(); ++i)
        {
            cv::putText(image, imageText[i], cv::Point(10,30 + target*60 + i*20), cv::FONT_HERSHEY_PLAIN, 1.0, color);
        }
    }

    averageTheta /= targetCount;

    cv::putText(image, fmt::format("Average Theta: {:03.1f}", averageTheta), cv::Point(10,30 + 400), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(255,255,255));


}