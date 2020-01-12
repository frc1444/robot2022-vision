#include <opencv2/opencv.hpp>

#include "InfiniteRechargeVision.h"
#include "Setup.h"
#include "VisionData.hpp"
#include "DataSender.h"

using namespace Lightning;

InfiniteRechargeVision::InfiniteRechargeVision(std::vector<spdlog::sink_ptr> sinks)
    : _isProcessorRunning(false)
    , _doProcessing(false)
{
    _logger = std::make_shared<spdlog::logger>("InfiniteRechargeVision", sinks.begin(), sinks.end());
    _logger->set_level(Lightning::Setup::Diagnostics::LogLevel);

    // Setup capture
    if (Setup::Camera::CameraId >= 0)
    {    
        if (Setup::Diagnostics::UseTestVideo)
        {
            _targetCapture = std::make_shared<cv::VideoCapture>(Setup::Diagnostics::TestVideoPath);
            _logger->info("Capture set to video: {0}", Setup::Diagnostics::TestVideoPath);
        }
        else if (Setup::Diagnostics::UseTestImage)
        {
            _targetCapture = std::make_shared<cv::VideoCapture>(Setup::Diagnostics::TestImagePath);
            _logger->info("Capture set to image(s): {0}", Setup::Diagnostics::TestImagePath);
        }
        else
        {
            _targetCapture = std::make_shared<cv::VideoCapture>(Setup::Camera::CameraId);
            _logger->info("Capture set to camera ID: {0}", Setup::Camera::CameraId);
        }
        
    }
    else
    {
        _logger->info("Capture will not be used");
    }

    // Setup processor
    if (_targetCapture)
    {
        if (_targetCapture->isOpened())
        {
            // TODO remove this
            cv::Vec3d offset(Setup::Processing::HatchOffset, 0, 0);

            _targetProcessor = std::make_unique<InfiniteRechargeProcessor>(sinks, "Main", _targetCapture, offset);  
        }
        else
        {           
            _logger->error("Failed to open capture.");          
        }
    }
    else
    {
        _logger->info("Processor will not be used");
    }  

    _dataSender = std::make_unique<DataSender>();
}

bool InfiniteRechargeVision::StartProcessing()
{
    if (_isProcessorRunning)
    {
        _logger->info("InfiniteRechargeVision is already running.");
        return false;
    }

    _doProcessing = true;

    std::thread t(&InfiniteRechargeVision::Process, this);

    if (t.joinable())
    {
        _isProcessorRunning = true;
        t.detach();
        return true;
    }

    return false;
}

void InfiniteRechargeVision::StopProcessing()
{
    _doProcessing = false;
}

void InfiniteRechargeVision::Process()
{
    _logger->debug("Enter Process thread");

    while (_doProcessing)
    {
        if (Setup::Diagnostics::ReadSetupFile)
        {
            Setup::LoadSetup();
        }

        std::vector<VisionData> targetData;

        if (_targetProcessor)
        {
            _targetProcessor->ProcessNextImage(targetData);
            // TODO check return? - shutdown after so any failed attempts?
        }

        // Apply robot-specific offsets

        // Pack results
        std::vector<VisionMessage> messages
        {
            VisionMessage { Setup::Camera::CameraId, targetData }
        };

        // Send results
        _dataSender->Send(messages);

        // Display diagnostic images, if desired
        if (Setup::Diagnostics::DisplayDebugImages)
        {
            if (_targetProcessor)
            {
                _targetProcessor->ShowDebugImages();
            }

            int key = cv::waitKey(Setup::Diagnostics::WaitKeyDelay);

            if (key == 27)
            {
                _logger->info("Escape key pressed. Shutting down.");
                _doProcessing = false;
            }
        }
    }

    _isProcessorRunning = false;

    _logger->debug("Leaving Process thread");
}