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

#include "MapPoint.h"
#include "ORBmatcher.h"
#include "Serializer.h"

#include<mutex>

namespace ORB_SLAM2
{

   mutex MapPoint::mGlobalMutex;

   MapPoint::MapPoint()
      : mnId(-1)
      , mnFirstKFid(-1)
      , nObs(0)
      , mnTrackReferenceForFrame(0)
      , mnLastFrameSeen(0)
      , mnBALocalForKF(0)
      , mnFuseCandidateForKF(0)
      , mnLoopPointForKF(0)
      , mnCorrectedByKF(0)
      , mnCorrectedReference(0)
      , mnBAGlobalForKF(0)
      , mpRefKF(static_cast<KeyFrame*>(NULL))
      , mnVisible(1)
      , mnFound(1)
      , mbBad(false)
      , mpReplaced(static_cast<MapPoint*>(NULL))
      , mfMinDistance(0)
      , mfMaxDistance(0)
   {
   }

   MapPoint::MapPoint(id_type id, const cv::Mat & worldPos, KeyFrame *pRefKF) 
      : mnId(id)
      , mnFirstKFid(pRefKF->GetId())
      , nObs(0)
      , mnTrackReferenceForFrame(0)
      , mnLastFrameSeen(0)
      , mnBALocalForKF(0)
      , mnFuseCandidateForKF(0)
      , mnLoopPointForKF(0)
      , mnCorrectedByKF(0)
      , mnCorrectedReference(0)
      , mnBAGlobalForKF(0)
      , mpRefKF(pRefKF)
      , mnVisible(1)
      , mnFound(1)
      , mbBad(false)
      , mpReplaced(static_cast<MapPoint*>(NULL))
      , mfMinDistance(0)
      , mfMaxDistance(0)
      , mNormalVector(cv::Mat::zeros(3, 1, CV_32F))
      , mWorldPos(worldPos)
   {
      if (worldPos.empty())
         throw exception("MapPoint::SetWorldPos([])!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
   }

   void MapPoint::SetWorldPos(const cv::Mat &Pos)
   {
      unique_lock<mutex> lock2(mGlobalMutex);
      unique_lock<mutex> lock(mMutexPos);
      if (Pos.empty())
         throw exception("MapPoint::SetWorldPos([])!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      Pos.copyTo(mWorldPos);
   }

   cv::Mat MapPoint::GetWorldPos()
   {
      unique_lock<mutex> lock(mMutexPos);
      return mWorldPos.clone();
   }

   cv::Mat MapPoint::GetNormal()
   {
      unique_lock<mutex> lock(mMutexPos);
      return mNormalVector.clone();
   }

   KeyFrame* MapPoint::GetReferenceKeyFrame()
   {
      unique_lock<mutex> lock(mMutexFeatures);
      return mpRefKF;
   }

   void MapPoint::AddObservation(KeyFrame* pKF, size_t idx)
   {
      unique_lock<mutex> lock(mMutexFeatures);
      if (mObservations.count(pKF))
         return;
      mObservations[pKF] = idx;

      if (pKF->right[idx] >= 0)
         nObs += 2;
      else
         nObs++;
   }

   void MapPoint::EraseObservation(KeyFrame* pKF, Map * pMap)
   {
      bool bBad = false;
      {
         unique_lock<mutex> lock(mMutexFeatures);
         if (mObservations.count(pKF))
         {
            int idx = mObservations[pKF];
            if (pKF->right[idx] >= 0)
               nObs -= 2;
            else
               nObs--;

            mObservations.erase(pKF);

            if (mpRefKF == pKF)
               mpRefKF = mObservations.begin()->first;

            // If only 2 observations or less, discard point
            if (nObs <= 2)
               bBad = true;
         }
      }

      if (bBad)
         SetBadFlag(pMap);
   }

   map<KeyFrame*, size_t> MapPoint::GetObservations()
   {
      unique_lock<mutex> lock(mMutexFeatures);
      return mObservations;
   }

   int MapPoint::Observations()
   {
      unique_lock<mutex> lock(mMutexFeatures);
      return nObs;
   }

   void MapPoint::SetBadFlag(Map * pMap)
   {
      map<KeyFrame*, size_t> obs;
      {
         unique_lock<mutex> lock1(mMutexFeatures);
         unique_lock<mutex> lock2(mMutexPos);
         mbBad = true;
         obs = mObservations;
         mObservations.clear();
      }
      for (map<KeyFrame*, size_t>::iterator mit = obs.begin(), mend = obs.end(); mit != mend; mit++)
      {
         KeyFrame* pKF = mit->first;
         pKF->EraseMapPointMatch(mit->second);
      }

      if (pMap)
         pMap->EraseMapPoint(this);
   }

   MapPoint* MapPoint::GetReplaced()
   {
      unique_lock<mutex> lock1(mMutexFeatures);
      unique_lock<mutex> lock2(mMutexPos);
      return mpReplaced;
   }

   void MapPoint::Replace(MapPoint* pMP, Map * pMap)
   {
      if (pMP->mnId == this->mnId)
         return;

      int nvisible, nfound;
      map<KeyFrame*, size_t> obs;
      {
         unique_lock<mutex> lock1(mMutexFeatures);
         unique_lock<mutex> lock2(mMutexPos);
         obs = mObservations;
         mObservations.clear();
         mbBad = true;
         nvisible = mnVisible;
         nfound = mnFound;
         mpReplaced = pMP;
      }

      for (map<KeyFrame*, size_t>::iterator mit = obs.begin(), mend = obs.end(); mit != mend; mit++)
      {
         // Replace measurement in keyframe
         KeyFrame* pKF = mit->first;

         if (!pMP->IsInKeyFrame(pKF))
         {
            pKF->ReplaceMapPointMatch(mit->second, pMP);
            pMP->AddObservation(pKF, mit->second);
         }
         else
         {
            pKF->EraseMapPointMatch(mit->second);
         }
      }
      pMP->IncreaseFound(nfound);
      pMP->IncreaseVisible(nvisible);
      pMP->ComputeDistinctiveDescriptors();

      if (pMap)
         pMap->EraseMapPoint(this);
   }

   bool MapPoint::isBad()
   {
      unique_lock<mutex> lock(mMutexFeatures);
      unique_lock<mutex> lock2(mMutexPos);
      return mbBad;
   }

   void MapPoint::IncreaseVisible(int n)
   {
      unique_lock<mutex> lock(mMutexFeatures);
      mnVisible += n;
   }

   void MapPoint::IncreaseFound(int n)
   {
      unique_lock<mutex> lock(mMutexFeatures);
      mnFound += n;
   }

   float MapPoint::GetFoundRatio()
   {
      unique_lock<mutex> lock(mMutexFeatures);
      return static_cast<float>(mnFound) / mnVisible;
   }

   void MapPoint::ComputeDistinctiveDescriptors()
   {
      // Retrieve all observed descriptors
      vector<cv::Mat> vDescriptors;

      map<KeyFrame*, size_t> observations;

      {
         unique_lock<mutex> lock1(mMutexFeatures);
         if (mbBad)
            return;
         observations = mObservations;
      }

      if (observations.empty())
         return;

      vDescriptors.reserve(observations.size());

      for (map<KeyFrame*, size_t>::iterator mit = observations.begin(), mend = observations.end(); mit != mend; mit++)
      {
         KeyFrame* pKF = mit->first;

         if (!pKF->isBad())
            vDescriptors.push_back(pKF->descriptors.row(mit->second));
      }

      if (vDescriptors.empty())
         return;

      // Compute distances between them
      const size_t N = vDescriptors.size();

      //float Distances[N][N];
      float * Distances = new float[N*N];
      for (size_t i = 0;i < N;i++)
      {
         //Distances[i][i]=0;
         Distances[i*N + i] = 0;
         for (size_t j = i + 1;j < N;j++)
         {
            int distij = ORBmatcher::DescriptorDistance(vDescriptors[i], vDescriptors[j]);
            //Distances[i][j]=distij;
            Distances[i*N + j] = distij;
            //Distances[j][i]=distij;
            Distances[j*N + i] = distij;
         }
      }

      // Take the descriptor with least median distance to the rest
      int BestMedian = INT_MAX;
      int BestIdx = 0;
      for (size_t i = 0;i < N;i++)
      {
         //vector<int> vDists(Distances[i],Distances[i]+N);
         vector<int> vDists(&Distances[i*N], &Distances[i*N] + N);
         sort(vDists.begin(), vDists.end());
         int median = vDists[0.5*(N - 1)];

         if (median < BestMedian)
         {
            BestMedian = median;
            BestIdx = i;
         }
      }

      delete[] Distances;

      {
         unique_lock<mutex> lock(mMutexFeatures);
         mDescriptor = vDescriptors[BestIdx].clone();
      }
   }

   cv::Mat MapPoint::GetDescriptor()
   {
      unique_lock<mutex> lock(mMutexFeatures);
      return mDescriptor.clone();
   }

   int MapPoint::GetIndexInKeyFrame(KeyFrame *pKF)
   {
      unique_lock<mutex> lock(mMutexFeatures);
      if (mObservations.count(pKF))
         return mObservations[pKF];
      else
         return -1;
   }

   bool MapPoint::IsInKeyFrame(KeyFrame *pKF)
   {
      unique_lock<mutex> lock(mMutexFeatures);
      return (mObservations.count(pKF));
   }

   void MapPoint::UpdateNormalAndDepth()
   {
      map<KeyFrame*, size_t> observations;
      KeyFrame* pRefKF;
      cv::Mat Pos;
      {
         unique_lock<mutex> lock1(mMutexFeatures);
         unique_lock<mutex> lock2(mMutexPos);
         if (mbBad)
            return;
         observations = mObservations;
         pRefKF = mpRefKF;
         Pos = mWorldPos.clone();
      }

      if (observations.empty())
         return;

      cv::Mat normal = cv::Mat::zeros(3, 1, CV_32F);
      int n = 0;
      for (map<KeyFrame*, size_t>::iterator mit = observations.begin(), mend = observations.end(); mit != mend; mit++)
      {
         KeyFrame* pKF = mit->first;
         cv::Mat Owi = pKF->GetCameraCenter();
         cv::Mat normali = mWorldPos - Owi;
         normal = normal + normali / cv::norm(normali);
         n++;
      }

      cv::Mat PC = Pos - pRefKF->GetCameraCenter();
      const float dist = cv::norm(PC);
      const int level = pRefKF->keysUn[observations[pRefKF]].octave;
      const float levelScaleFactor = pRefKF->scaleFactors[level];
      const int nLevels = pRefKF->scaleLevels;

      {
         unique_lock<mutex> lock3(mMutexPos);
         mfMaxDistance = dist * levelScaleFactor;
         mfMinDistance = mfMaxDistance / pRefKF->scaleFactors[nLevels - 1];
         mNormalVector = normal / n;
      }
   }

   float MapPoint::GetMinDistanceInvariance()
   {
      unique_lock<mutex> lock(mMutexPos);
      return 0.8f*mfMinDistance;
   }

   float MapPoint::GetMaxDistanceInvariance()
   {
      unique_lock<mutex> lock(mMutexPos);
      return 1.2f*mfMaxDistance;
   }

   int MapPoint::PredictScale(const float &currentDist, KeyFrame* pKF)
   {
      float ratio;
      {
         unique_lock<mutex> lock(mMutexPos);
         ratio = mfMaxDistance / currentDist;
      }

      int nScale = ceil(log(ratio) / pKF->logScaleFactor);
      if (nScale < 0)
         nScale = 0;
      else if (nScale >= pKF->scaleLevels)
         nScale = pKF->scaleLevels - 1;

      return nScale;
   }

   int MapPoint::PredictScale(const float &currentDist, Frame* pF)
   {
      float ratio;
      {
         unique_lock<mutex> lock(mMutexPos);
         ratio = mfMaxDistance / currentDist;
      }

      int nScale = ceil(log(ratio) / pF->mfLogScaleFactor);
      if (nScale < 0)
         nScale = 0;
      else if (nScale >= pF->mnScaleLevels)
         nScale = pF->mnScaleLevels - 1;

      return nScale;
   }

   id_type MapPoint::GetId()
   {
      return mnId;
   }

   size_t MapPoint::GetBufferSize()
   {
      size_t size = sizeof(MapPoint::Header);
      size += Serializer::GetMatBufferSize(mWorldPos);
      size += Serializer::GetMatBufferSize(mNormalVector);
      size += Serializer::GetMatBufferSize(mDescriptor);
      size += sizeof(size_t) + mObservations.size() * (sizeof(Observation));
      return size;
   }

   void * MapPoint::ReadBytes(const void * buffer, Map & map)
   {
      MapPoint::Header * pHeader = (MapPoint::Header *)buffer;
      mnId = pHeader->mnId;
      mnFirstKFid = pHeader->mnFirstKFId;
      nObs = pHeader->nObs;
      mpRefKF = map.GetKeyFrame(pHeader->mpRefKFId);
      mnVisible = pHeader->mnVisible;
      mnFound = pHeader->mnFound;
      mbBad = pHeader->mbBad;
      mpReplaced = map.GetMapPoint(pHeader->mpReplacedId);
      mfMinDistance = pHeader->mfMinDistance;
      mfMaxDistance = pHeader->mfMaxDistance;

      // read variable-length data
      char * pData = (char *)(pHeader + 1);
      pData = (char *)Serializer::ReadMatrix(pData, mWorldPos);
      pData = (char *)Serializer::ReadMatrix(pData, mNormalVector);
      pData = (char *)Serializer::ReadMatrix(pData, mDescriptor);
      pData = (char *)ReadObservations(pData, map, mObservations);
      return pData;
   }

   void * MapPoint::WriteBytes(const void * buffer)
   {
      MapPoint::Header * pHeader = (MapPoint::Header *)buffer;
      pHeader->mnId = mnId;
      pHeader->mnFirstKFId = mnFirstKFid;
      pHeader->nObs = nObs;
      pHeader->mpRefKFId = mpRefKF->GetId();
      pHeader->mnVisible = mnVisible;
      pHeader->mnFound = mnFound;
      pHeader->mbBad = mbBad;
      pHeader->mpReplacedId = mpReplaced->GetId();
      pHeader->mfMinDistance = mfMinDistance;
      pHeader->mfMaxDistance = mfMaxDistance;

      // write variable-length data
      char * pData = (char *)(pHeader + 1);
      pData = (char *)Serializer::WriteMatrix(pData, mWorldPos);
      pData = (char *)Serializer::WriteMatrix(pData, mNormalVector);
      pData = (char *)Serializer::WriteMatrix(pData, mDescriptor);
      pData = (char *)WriteObservations(pData, mObservations);
      return pData;
   }

   void * MapPoint::ReadObservations(const void * buffer, Map & map, std::map<KeyFrame *, size_t> & observations)
   {
      observations.clear();
      size_t * pQuantity = (size_t *)buffer;
      Observation * pData = (Observation *)(pQuantity + 1);
      Observation * pEnd = pData + *pQuantity;
      while (pData < pEnd)
      {
         KeyFrame * pKF = map.GetKeyFrame(pData->keyFrameId);
         observations[pKF] = pData->index;
         ++pData;
      }
      return pData;
   }

   void * MapPoint::WriteObservations(const void * buffer, std::map<KeyFrame *, size_t> & observations)
   {
      size_t * pQuantity = (size_t *)buffer;
      *pQuantity = observations.size();
      Observation * pData = (Observation *)(pQuantity + 1);
      for (std::pair<KeyFrame *, size_t> p : observations)
      {
         pData->keyFrameId = p.first->GetId();
         pData->index = p.second;
         ++pData;
      }
      return pData;
   }

} //namespace ORB_SLAM
