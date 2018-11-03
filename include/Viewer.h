/**
* This file is part of ORB-SLAM2-TEAM.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
* Copyright (C) 2018 Joe Bedard <mr dot joe dot bedard at gmail dot com>
* For more information see <https://github.com/joebedard/ORB_SLAM2_TEAM>
*
* ORB-SLAM2-TEAM is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2-TEAM is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2-TEAM. If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef VIEWER_H
#define VIEWER_H

#include "FrameDrawer.h"
#include "MapDrawer.h"
#include "Tracking.h"
#include "SyncPrint.h"
#include "Mapper.h"

#include <mutex>

namespace ORB_SLAM2
{

   class Tracking;
   class FrameDrawer;
   class MapDrawer;

   class Viewer : SyncPrint
   {
   public:
      Viewer(FrameDrawer * pFrameDrawer, MapDrawer * pMapDrawer, Tracking * pTracking, Mapper * pMapper, bool embeddedFrameDrawer = false);

      Viewer(vector<FrameDrawer *> vFrameDrawers, vector<MapDrawer *> vMapDrawers, vector<Tracking *> vTrackers, Mapper * pMapper, bool embeddedFrameDrawers = true);

      // Main thread function. Draw points, keyframes, the current camera pose and the last processed
      // frame. Drawing is refreshed according to the camera fps. We use Pangolin.
      void Run();

      void RequestFinish();

      bool CheckFinish();

      void RequestStop();

      bool isFinished();

      bool isStopped();

      void Release();

   private:

      string mWindowTitle;

      bool mEmbeddedFrameDrawers;

      bool Stop();

      vector<FrameDrawer*> mvFrameDrawers;
      vector<MapDrawer *> mvMapDrawers;
      vector<Tracking*> mvTrackers;
      Mapper * mpMapper;

      void SetFinish();
      bool mbFinishRequested;
      bool mbFinished;
      std::mutex mMutexFinish;

      bool mbStopped;
      bool mbStopRequested;
      std::mutex mMutexStop;

      bool mbResetting;
   };

}


#endif // VIEWER_H


