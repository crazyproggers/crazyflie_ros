#include <tf/transform_listener.h>
#include <std_srvs/Empty.h>
#include <cmath>
#include "goals_publisher.h"


constexpr double degToRad(double deg) {
    return deg / 180.0 * M_PI;
}

// Static variables
std::atomic<uint> GoalsPublisher::m_amountCrazyfliesAtAnchors(0);
uint GoalsPublisher::m_totalCrazyflies = 0;


GoalsPublisher::GoalsPublisher(
    const std::string &worldFrame,
    const std::string &frame,
    uint publishRate)
    : m_nh                        ()
    , m_worldFrame                (worldFrame)
    , m_frame                     (frame)
    , m_publisher                 ()
    , m_subscriber                ()
    , m_transformListener         ()
    , m_loopRate                  (publishRate)
    , m_errMutex                  ()
{
    m_transformListener.waitForTransform(m_worldFrame, m_frame, ros::Time(0), ros::Duration(5.0));
    m_publisher = m_nh.advertise<geometry_msgs::PoseStamped>(m_frame + "/goal", 1);
    m_totalCrazyflies++; // register new crazyflie
}


inline Goal GoalsPublisher::getPosition() {
    ros::Time commonTime;
    tf::StampedTransform position;
    std::string errMsg;
    
    m_transformListener.getLatestCommonTime(m_worldFrame, m_frame, commonTime, &errMsg);

    if (errMsg.length()) {
        std::lock_guard<std::mutex> locker(m_errMutex);
        ROS_ERROR("%s", errMsg.c_str());
    }

    if (m_transformListener.canTransform(m_worldFrame, m_frame, commonTime, &errMsg))
        m_transformListener.lookupTransform(m_worldFrame, m_frame, commonTime, position);
    else throw;

    if (errMsg.length()) {
        std::lock_guard<std::mutex> locker(m_errMutex);
        ROS_ERROR("%s", errMsg.c_str());
    }

    double x = position.getOrigin().x();
    double y = position.getOrigin().y();
    double z = position.getOrigin().z();

    double roll, pitch, yaw;
    tf::Quaternion quaternion = position.getRotation();
    tf::Matrix3x3(quaternion).getRPY(roll, pitch, yaw);

    return Goal(x, y, z, roll, pitch, yaw);
}


inline bool GoalsPublisher::goalIsReached(const Goal &position, const Goal &goal) const {
    return (fabs(position.x()     - goal.x()) < 0.3) &&
           (fabs(position.y()     - goal.y()) < 0.3) &&
           (fabs(position.z()     - goal.z()) < 0.3) &&
           (fabs(position.roll()  - goal.roll())  < degToRad(10)) &&
           (fabs(position.pitch() - goal.pitch()) < degToRad(10)) &&
           (fabs(position.yaw()   - goal.yaw())   < degToRad(10));
}


void GoalsPublisher::run(std::vector<Goal> path, bool synchAtAnchors) {
    for (Goal goal: path) {
        while (ros::ok()) {
            m_publisher.publish(goal.getMsg());

            try {
                Goal position = getPosition();
            
                if (goalIsReached(position, goal)) {
                    ros::Duration(goal.delay()).sleep();
                    break; // go to next goal
                }
            }
            catch(...) {
                ROS_ERROR("%s", "Could not get current position!");
            } 

            m_loopRate.sleep();
        } // while (ros::ok())

        if (synchAtAnchors && goal.isAnchor()) {
            if (m_amountCrazyfliesAtAnchors == m_totalCrazyflies)
                m_amountCrazyfliesAtAnchors = 0;
            m_amountCrazyfliesAtAnchors++;

            while (ros::ok()) {
                m_publisher.publish(goal.getMsg());
                if (m_amountCrazyfliesAtAnchors == m_totalCrazyflies) break;
                m_loopRate.sleep();
            }
        } // if (synchAtAnchors && goal.isAnchor())
    } // for (Goal goal: path)

    std_srvs::Empty empty_srv;
    ros::service::call(m_frame + "/land", empty_srv);
} // run(std::vector<Goal> path)


void GoalsPublisher::run(double frequency) {
    m_subscriber = m_nh.subscribe("/swarm/commands", 1, &GoalsPublisher::directionChanged, this);
    ros::Timer timer = m_nh.createTimer(ros::Duration(1.0/frequency), &GoalsPublisher::goToGoal, this);

    ros::Rate loop_rate(10); // 10 Hz
    while (ros::ok) {
        ros::spinOnce();
        loop_rate.sleep();
    }
}


void GoalsPublisher::directionChanged(const std_msgs::Byte::ConstPtr &direction) {
    m_direction = direction->data;
}


Goal GoalsPublisher::getNewGoal(const Goal &oldGoal) {
     //* Directions:
     //* forward          -- Y += 0.01;
     //* backward         -- Y -= 0.01;
     //* rightward        -- X += 0.01;
     //* leftward         -- X -= 0.01;
     //* upward           -- Z += 0.01;
     //* downward         -- Z -= 0.01;
     //
    double x        = oldGoal.x();
    double y        = oldGoal.y();
    double z        = oldGoal.y();
    double roll     = oldGoal.roll();
    double pitch    = oldGoal.pitch();
    double yaw      = oldGoal.yaw();

    double step     = 0.1;

    if (m_direction == DIRECTION::forward)
        ;
    else if (m_direction == DIRECTION::backward)
        ;
    else if (m_direction == DIRECTION::rightward)
        ;
    else if (m_direction == DIRECTION::leftward)
        ;
    else if (m_direction == DIRECTION::upward)
        z += step;
    else if (m_direction == DIRECTION::downward)
        z = (z - step)? z : z - step;

    m_direction = 0;
    
    return Goal(x, y, z, roll, pitch, yaw);
}


void GoalsPublisher::goToGoal(const ros::TimerEvent &e) {
    if (!m_direction) return;

    // Create the path
    std::vector<Goal> path;
    try {
        Goal goal1 = getPosition();
        Goal goal2 = getNewGoal(goal1);
        //path = std::move(interpolate(goal1, goal2));
    }
    catch(...) {
        ROS_ERROR("%s", "Could not get current position!");
    } 

    for (size_t i = 0; i < path.size() - 1; ++i) {
        Goal &goal = path[i];

        while (ros::ok()) {
            if (m_direction != 0) return; // interupt from world

            m_publisher.publish(goal.getMsg());
            
            try {
                Goal position = getPosition();
            
                if (goalIsReached(position, goal))
                    break; // go to next goal
            }
            catch(...) {
                ROS_ERROR("%s", "Could not get current position!");
            } 
         
            m_loopRate.sleep();
        } // while (ros::ok())
    }// for (Goal goal: path)
    
    // Hovering at last goal
    Goal &goal = path[path.size()-1];
    while (ros::ok()) {
        // interupt from world or path does not exist
        if (m_direction != 0 || !path.size()) return;

        m_publisher.publish(goal.getMsg());
        m_loopRate.sleep();
    } // while (ros::ok())
}

