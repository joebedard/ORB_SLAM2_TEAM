/**
* This file is part of ORB-SLAM2.
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/

#include "MapperClient.h"
#include "Optimizer.h"
#include "Sleep.h"
#include <exception>

namespace ORB_SLAM2
{

   MapperClient::MapperClient(Map * pMap, ORBVocabulary* pVocab, const bool bMonocular) :
      SyncPrint("MapperClient: "), mpMap(pMap), mpVocab(pVocab), 
      mbMonocular(bMonocular), mInitialized(false)
   {
      if (pMap == NULL)
         throw std::exception("pMap must not be NULL");

      if (pVocab == NULL)
         throw std::exception("pVocab must not be NULL");

      mpKeyFrameDB = new KeyFrameDatabase(*pVocab);

      ResetTrackerStatus();

      //Initialize and start the Local Mapping thread
      mpLocalMapper = new LocalMapping(mpMap, mpKeyFrameDB, mbMonocular, FIRST_MAPPOINT_ID_LOCALMAPPER, MAPPOINT_ID_SPAN);
      mptLocalMapping = new thread(&ORB_SLAM2::LocalMapping::Run, mpLocalMapper);

      //Initialize and start the Loop Closing thread
      mpLoopCloser = new LoopClosing(mpMap, mpKeyFrameDB, mpVocab, !mbMonocular);
      mptLoopClosing = new thread(&ORB_SLAM2::LoopClosing::Run, mpLoopCloser);

      mpLocalMapper->SetLoopCloser(mpLoopCloser);
      mpLoopCloser->SetLocalMapper(mpLocalMapper);
   }

   long unsigned MapperClient::KeyFramesInMap()
   {
      return mpMap->KeyFramesInMap();
   }

   void MapperClient::Reset()
   {
      unique_lock<mutex> lock(mpMap->mMutexMapUpdate);

      // Reset Local Mapping
      Print("Begin Local Mapper Reset");
      mpLocalMapper->RequestReset();
      Print("End Local Mapper Reset");

      // Reset Loop Closing
      Print("Begin Loop Closing Reset");
      mpLoopCloser->RequestReset();
      Print("End Loop Closing Reset");

      NotifyReset();

      // Clear BoW Database
      Print("Begin Database Reset");
      mpKeyFrameDB->clear();
      Print("End Database Reset");

      // Clear Map (this erase MapPoints and KeyFrames)
      Print("Begin Map Reset");
      mpMap->Clear();
      Print("End Map Reset");

      ResetTrackerStatus();
      mInitialized = false;
      Print("Reset Complete");
   }

   std::vector<KeyFrame*> MapperClient::DetectRelocalizationCandidates(Frame* F)
   {
      return mpKeyFrameDB->DetectRelocalizationCandidates(F);
   }
      
   bool MapperClient::GetInitialized()
   {
      return mInitialized;
   }

   bool MapperClient::GetPauseRequested()
   {
      return mpLocalMapper->PauseRequested();
   }
   
   bool MapperClient::AcceptKeyFrames()
   {
      return mpLocalMapper->AcceptKeyFrames();
   }
   
   void MapperClient::Shutdown()
   {
      mpLocalMapper->RequestFinish();
      mpLoopCloser->RequestFinish();

      // Wait until all thread have effectively stopped
      while (!mpLocalMapper->isFinished() || !mpLoopCloser->isFinished() || mpLoopCloser->isRunningGBA())
      {
         sleep(5000);
      }
   }

   void MapperClient::Initialize(unsigned int trackerId)
   {
      if (mInitialized)
         throw exception("The mapper may only be initialized once.");

      if (trackerId != 0)
         throw exception("Only the first Tracker (id=0) may initialize the map.");

      auto allKFs = mpMap->GetAllKeyFrames();
      for (auto it = allKFs.begin(); it != allKFs.end(); it++)
      {
         if (!InsertKeyFrame(trackerId, *it))
             throw exception("Unable to InsertKeyFrame during Initialize.");
      }

      mInitialized = true;
   }

   bool MapperClient::InsertKeyFrame(unsigned int trackerId, KeyFrame *pKF)
   {
      if (mpLocalMapper->InsertKeyFrame(pKF))
      {
         // update TrackerStatus array with next KeyFrameId and MapPointId

         assert((pKF->GetId() - trackerId) % KEYFRAME_ID_SPAN == 0);
         if (mTrackers[trackerId].nextKeyFrameId <= pKF->GetId())
         {
            mTrackers[trackerId].nextKeyFrameId = pKF->GetId() + KEYFRAME_ID_SPAN;
         }

         if (!mbMonocular)
         {
             // stereo and RGBD modes will create MapPoints
             set<ORB_SLAM2::MapPoint*> mapPoints = pKF->GetMapPoints();
             for (auto pMP : mapPoints)
             {
                 if (!pMP || pMP->Observations() < 1)
                 {
                    assert((pMP->GetId() - trackerId) % MAPPOINT_ID_SPAN == 0);
                    if (mTrackers[trackerId].nextMapPointId <= pMP->GetId())
                    {
                        mTrackers[trackerId].nextMapPointId = pMP->GetId() + MAPPOINT_ID_SPAN;
                    }
                 }
             }
         }

         return true;
      }
      else
         return false;
   }

   unsigned int MapperClient::LoginTracker(unsigned long  & firstKeyFrameId, unsigned int & keyFrameIdSpan, unsigned long & firstMapPointId, unsigned int & mapPointIdSpan)
   {
      unique_lock<mutex> lock(mMutexLogin);

      unsigned int id;
      for (id = 0; id < MAX_TRACKERS; ++id)
      {
         if (!mTrackers[id].connected)
         {
            mTrackers[id].connected = true;
            break;
         }
      }

      if (id >= MAX_TRACKERS)
         throw std::exception("Maximum number of trackers reached. Additional trackers are not supported.");

      firstKeyFrameId = mTrackers[id].nextKeyFrameId;
      keyFrameIdSpan = KEYFRAME_ID_SPAN;
      firstMapPointId = mTrackers[id].nextMapPointId;
      mapPointIdSpan = MAPPOINT_ID_SPAN;

      return id;
   }

   void MapperClient::LogoutTracker(unsigned int id)
   {
      mTrackers[id].connected = false;
   }

   Map * MapperClient::GetMap()
   {
       return mpMap;
   }

   void MapperClient::AddObserver(Observer * ob)
   {
       mObservers[ob] = ob;
   }

   void MapperClient::RemoveObserver(Observer * ob)
   {
       mObservers.erase(ob);
   }

   void MapperClient::NotifyReset()
   {
      for (auto it : mObservers)
      {
         it.second->HandleReset();
      }
   }

   void MapperClient::ResetTrackerStatus()
   {
      unique_lock<mutex> lock(mMutexLogin);
      for (int i = 0; i < MAX_TRACKERS; ++i)
      {
         mTrackers[i].connected = false;
         mTrackers[i].nextKeyFrameId = i;
         mTrackers[i].nextMapPointId = i;
      }
   }

}