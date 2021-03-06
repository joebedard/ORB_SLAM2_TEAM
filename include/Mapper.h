/**
* This file is part of ORB-SLAM2-TEAM.
*
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

#ifndef MAPPER_H
#define MAPPER_H

#include "Map.h"
#include "MapperSubject.h"
#include "KeyFrame.h"

namespace ORB_SLAM2_TEAM
{

   /*
   base interface for all Mapping functionality as in the Proxy design pattern
   see https://sourcemaking.com/design_patterns/proxy
   */
   class Mapper : public MapperSubject
   {
   public:

      virtual long unsigned  KeyFramesInMap() = 0;

      virtual long unsigned MapPointsInMap() = 0;

      virtual unsigned int LoopsInMap() = 0;

      virtual void Reset() = 0;

      virtual std::vector<KeyFrame *> DetectRelocalizationCandidates(Frame * F) = 0;

      virtual bool GetPauseRequested() = 0;

      virtual bool GetIdle() = 0;

      virtual bool InsertKeyFrame(unsigned int trackerId, KeyFrame * pKF, vector<MapPoint *> & createdMapPoints, vector<MapPoint *> & updatedMapPoints) = 0;

      virtual void InitializeMono(unsigned int trackerId, vector<MapPoint *> & mapPoints, KeyFrame * pKF1, KeyFrame * pKF2) = 0;

      virtual void InitializeStereo(unsigned int trackerId, vector<MapPoint *> & mapPoints, KeyFrame * pKF) = 0;

      virtual bool GetInitialized() = 0;

      virtual Map & GetMap() = 0;

      virtual std::mutex & GetMutexMapUpdate() = 0;

      virtual void LoginTracker(
         const cv::Mat & pivotCalib,
         unsigned int & trackerId,
         id_type  & firstKeyFrameId,
         unsigned int & keyFrameIdSpan,
         id_type & firstMapPointId,
         unsigned int & mapPointIdSpan) = 0;

      virtual void LogoutTracker(unsigned int id) = 0;

      virtual void UpdatePose(unsigned int trackerId, const cv::Mat & poseTcw) = 0;

      virtual vector<cv::Mat> GetTrackerPoses() = 0;

      virtual vector<cv::Mat> GetTrackerPivots() = 0;

   };
}

#endif // MAPPER_H