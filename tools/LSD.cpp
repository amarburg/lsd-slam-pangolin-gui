/**
*
* Based on original LSD-SLAM code from:
* Copyright 2013 Jakob Engel <engelj at in dot tum dot de> (Technical University of Munich)
* For more information see <http://vision.in.tum.de/lsdslam>
*
* LSD-SLAM is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* LSD-SLAM is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with LSD-SLAM. If not, see <http://www.gnu.org/licenses/>.
*/

#include <boost/thread.hpp>

#include "libg3logger/g3logger.h"

#include "SlamSystem.h"

#include "util/settings.h"
#include "util/Parse.h"
#include "util/globalFuncs.h"
#include "util/Configuration.h"

#include "GUI.h"
#include "Pangolin_IOWrapper/PangolinOutput3DWrapper.h"
#include "Pangolin_IOWrapper/PangolinOutputIOWrapper.h"

#include "libvideoio/ImageSource.h"
#include "libvideoio/Undistorter.h"

#include "libvideoio-go/FrameSet.h"

#include "CLI11.hpp"

#include <App/InputThread.h>
#include <App/App.h>

#include "Input.h"


using namespace lsd_slam;
using namespace libvideoio;

using std::string;

int main( int argc, char** argv )
{
  // Initialize the logging system
  libg3logger::G3Logger logWorker( argv[0] );
  logWorker.logBanner();

  CLI::App app;

  // Add new options/flags here
  std::string calibFile;
  app.add_option("-c,--calib", calibFile, "Calibration file" )->required()->check(CLI::ExistingFile);

  bool verbose;
  app.add_flag("-v,--verbose", verbose, "Print DEBUG output to console");

  bool noGui;
  app.add_flag("--no-gui", noGui, "Don't display GUI");

  std::string chunk;
  app.add_option("--chunk", chunk, "Chunk");

  std::vector<std::string> inFiles;
  app.add_option("--input,input", inFiles, "Input files or directories");

  int skip = 0;
  app.add_option("--skip", skip, "Skip frames");

  // Defines the configuration file;  see
  //    https://cliutils.gitlab.io/CLI11Tutorial/chapters/config.html
  app.set_config("--config");

  CLI11_PARSE(app, argc, argv);

  std::shared_ptr<ImageSource> dataSource;
  fs::path setPath( inFiles.front() );

  if( setPath.extension() == ".json" ) {
    auto frameSet( new libvideoio::FrameSet( setPath.string() ) );
    if( !frameSet->isOpened() ) {
      LOG(FATAL) << " Failed to open " << setPath << " as a Go FrameSet";
    }

    if( chunk.size() > 0 && !frameSet->openChunk( chunk ) ) {
      LOG(FATAL) << "Unable to find chunk " << chunk << " in frameset " << setPath;
    }

    if( skip > 0 ) frameSet->setSkip( skip );

    dataSource.reset( frameSet );

    dataSource->setFPS( 30 ); //fpsArg.getValue() );
    dataSource->setOutputType( CV_8UC1 );

  } else {
    dataSource = Input::makeInput( inFiles );
  }

  CHECK((bool)dataSource) << "Data source is null";

//  std::shared_ptr<Undistorter> cropper( new ImageCropper( 1920, 1024, 0, 0, undistorter ) );

  std::shared_ptr<Undistorter> shrinker( new ImageResizer( 640, 360 ) );
  std::shared_ptr<Undistorter> undistorter(libvideoio::UndistorterFactory::getUndistorterFromFile( calibFile, shrinker ));
  CHECK((bool)undistorter) << "Undistorter shouldn't be null";
  std::shared_ptr<Undistorter> cropper( new ImageCropper( 640, 320, 00, 20, undistorter ) );

  logWorker.verbose( verbose );

  // Load configuration for LSD-SLAM
  lsd_slam::Configuration conf;
  conf.inputImage = undistorter->inputImageSize();
  conf.slamImage  = cropper->outputImageSize();
  conf.camera     = undistorter->getCamera();

  LOG(INFO) << "Slam image: " << conf.slamImage.width << " x " << conf.slamImage.height;
  CHECK( (conf.camera.fx) > 0 && (conf.camera.fy > 0) ) << "Camera focal length is zero";

  std::shared_ptr<SlamSystem> system( new SlamSystem(conf) );

  // GUI need to be initialized in main thread on OSX,
  // so run GUI elements in the main thread.
  std::shared_ptr<GUI> gui( nullptr );
  std::shared_ptr<PangolinOutputIOWrapper> ioWrapper(nullptr);

  if( !noGui ) {
    gui.reset( new GUI( system->conf() ) );
    lsd_slam::PangolinOutput3DWrapper *outputWrapper = new PangolinOutput3DWrapper( system->conf(), *gui );
    system->set3DOutputWrapper( outputWrapper );

    ioWrapper.reset( new PangolinOutputIOWrapper( system->conf(), *gui ));
  }

  InputThread input( system, dataSource, cropper );
  input.setIOOutputWrapper( ioWrapper );

  LOG(INFO) << "Starting input thread.";
  boost::thread inputThread( boost::ref(input) );

  // Wait for all threads to indicate they are ready to go
  input.inputReady.wait();

  LOG(INFO) << "Starting all threads.";
  startAll.notify();

  if( gui ) {
    while(!pangolin::ShouldQuit() && !input.inputDone.getValue() )
    {
      if( gui ) gui->update();
    }
  } else {
    input.inputDone.wait();
  }

  LOG(INFO) << "Finalizing system.";
  system->finalize();

  return 0;
}
