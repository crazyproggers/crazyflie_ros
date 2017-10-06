#ifndef GOALS_PUBLISHER_H
#define GOALS_PUBLISHER_H

#include <tf/transform_listener.h>
#include <std_msgs/Byte.h>
#include <atomic>
#include <mutex>
#include "goal.h"


class GoalsPublisher {
    typedef unsigned int uint;

    ros::NodeHandle          m_nh;
    std::string              m_worldFrame;
    std::string              m_frame;
    ros::Publisher           m_publisher;
    ros::Subscriber          m_subscriber;
    tf::TransformListener    m_transformListener;
    ros::Rate                m_loopRate;
    std::mutex               m_errMutex;

    static uint              m_totalCrazyflies;
    static std::atomic<uint> m_amountCrazyfliesAtAnchors;

    short                    m_direction;

private:
    enum DIRECTION {
        forward     = 1,
        backward    = 2,
        rightward   = 3,
        leftward    = 4,
        upward      = 5,
        downward    = 6,
    };


    // Get current position
    inline Goal getPosition();

    // Checking that |position - goal| < E
    inline bool goalIsReached(const Goal &position, const Goal &goal) const;

    // Create a new goal on current direction and old goal
    inline Goal getNewGoal(const Goal &oldGoal);

    // Subscriber callback
    void directionChanged(const std_msgs::Byte::ConstPtr &direction);

    // Timer callback
    void goToGoal(const ros::TimerEvent &e);

public:
    GoalsPublisher() = delete;
    GoalsPublisher(const GoalsPublisher &) = delete;
    GoalsPublisher & operator=(const GoalsPublisher &) = delete;

    GoalsPublisher(const std::string &worldFrame, 
                   const std::string &frame,
                   uint publishRate);

    /*
     * Automatic flight
     *
     * If mode "synchronization at anchors" is enabled crazyflies that are located at anchor points
     * begin to wait lagging copters.
     */ 
    void run(std::vector<Goal> path, bool synchAtAnchors = false);
    
    // Controlled flight
    void run(double frequency);
};

#endif // GOALS_PUBLISHER_H
