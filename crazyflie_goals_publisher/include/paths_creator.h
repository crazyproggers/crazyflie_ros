#ifndef PATHS_creator_H
#define PATHS_creator_H

#include <goal.h>

class PathsCreator {
    typedef unsigned int uint;

    std::vector<std::vector<Goal>> m_goalsTable;
    std::string m_worldFrame;
    std::vector<std::string> m_frames;

    bool readGoals(const std::string &mapPath);
    void interpolate(double distanceBetweenDots, bool splinesMode);

public:
    PathsCreator() = delete;
    PathsCreator(const PathsCreator &) = delete;
    PathsCreator & operator=(const PathsCreator &) = delete;

    PathsCreator(const  std::string &worldFrame,
                 const  std::vector<std::string> &frames,
                 const  std::string &mapPath,
                 double distanceBetweenDots = 0.01,
                 bool   splinesMode = false);

    std::vector<Goal> getPath(const std::string &frame) const;

    ~PathsCreator();
};

#endif // PATHS_creator_H
