#pragma once

#include "json.hpp"

#include "VisionStatus.hpp"

namespace Lightning
{

class VisionData
{
public:

   // TODO define id better

    VisionStatus status;
    int targetId;
    double x;
    double y;
    double z;
    double roll;
    double pitch;
    double yaw;
    double imageX;
    double imageY;
    double theta;
    double dist;
};

inline void to_json(nlohmann::json& j, const VisionData& d) {
    j = nlohmann::json{{"status", (int)d.status}, 
    {"x_m", d.x}, 
    {"y_m", d.y}, 
    {"z_m", d.z}, 
    {"roll_deg", d.roll}, 
    {"pitch_deg", d.pitch}, 
    {"yaw_deg", d.yaw}, 
    {"imageX_px", d.imageX}, 
    {"imageY_px", d.imageY}, 
    {"theta_deg", d.theta},
    {"dist_m", d.dist}};
}

inline void from_json(const nlohmann::json& j, VisionData& d) {
    j.at("status").get_to(d.status);
    j.at("x_m").get_to(d.x);
    j.at("y_m").get_to(d.y);
    j.at("z_m").get_to(d.z);
    j.at("roll_deg").get_to(d.roll);
    j.at("pitch_deg").get_to(d.pitch);
    j.at("yaw_deg").get_to(d.yaw);
    j.at("imageX_px").get_to(d.imageX);
    j.at("imageY_px").get_to(d.imageY);
    j.at("theta_deg").get_to(d.theta);
    j.at("dist_deg").get_to(d.dist);


}

class VisionMessage
{
public:
    int cameraId;
    std::vector<VisionData> packets;

};

inline void to_json(nlohmann::json& j, const VisionMessage& d) {
    j = nlohmann::json{{"cameraId", (int)d.cameraId}, {"packets", d.packets}};
}

inline void from_json(const nlohmann::json& j, VisionMessage& d) {
    j.at("cameraId").get_to(d.cameraId);
    j.at("packets").get_to(d.packets);
}

}