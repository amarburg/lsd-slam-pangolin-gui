#pragma once

#include "IOWrapper/OutputIOWrapper.h"

#include "util/ThreadMutexObject.h"

#include "SlamSystem.h"

#include "libvideoio/ImageSource.h"
#include "libvideoio/Undistorter.h"

#include "InputThread.h"

namespace lsd_slam {

  class StereoInputThread : public InputThread {
  public:

    StereoInputThread( const std::shared_ptr<lsd_slam::SlamSystem> &system,
                   const std::shared_ptr<libvideoio::ImageSource> &dataSource,
                   const std::shared_ptr<libvideoio::Undistorter> &leftUndistorter,
                   const std::shared_ptr<libvideoio::Undistorter> &rightUndistorter,
                    const Sophus::SE3d &rightToLeft );

    // Entry point for boost::thread
    void operator()();

    // Uses InputThread::undistorter for leftUndistorter
    std::shared_ptr<libvideoio::Undistorter> rightUndistorter;

  protected:

    Sophus::SE3d _rightToLeft;

  };
}
