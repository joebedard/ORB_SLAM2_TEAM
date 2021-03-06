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

#include "MapperServer.h"
#include "Sleep.h"
#include "System.h"
#include "Converter.h"
#include <pangolin/pangolin.h>

namespace ORB_SLAM2_TEAM
{

   System::System(
      const string & vocabFilename,
      const string & settingsFilename,
      const SensorType sensor,
      const bool bEmbeddedVideo,
      const bool bUseViewer
   ) :
      mSensor(sensor),
      mpViewer(static_cast<Viewer*>(NULL)),
      mpMapper(static_cast<Mapper*>(NULL))
   {
      // Output welcome message
      stringstream ss1;
      ss1 << endl;
      ss1 << "   ORB-SLAM2-TEAM" << endl;
      ss1 << "   Copyright (C) 2014-2016 Raul Mur-Artal, University of Zaragoza" << endl;
      ss1 << "   Copyright (C) 2018 Joe Bedard" << endl;
      ss1 << "   This program comes with ABSOLUTELY NO WARRANTY;" << endl;
      ss1 << "   This is free software, and you are welcome to redistribute it" << endl;
      ss1 << "   under certain conditions. See LICENSE.txt." << endl;
      Print(ss1);

      stringstream ss2;
      ss2 << "Input sensor was set to: ";
      if (mSensor == MONOCULAR)
         ss2 << "Monocular";
      else if (mSensor == STEREO)
         ss2 << "Stereo";
      else if (mSensor == RGBD)
         ss2 << "RGB-D";
      Print(ss2);

      //Check settings file
      cv::FileStorage settings(settingsFilename.c_str(), cv::FileStorage::READ);
      if (!settings.isOpened())
      {
         string m("Failed to open settings file at: ");
         m.append(settingsFilename);
         throw exception(m.c_str());
      }

      //Load ORB Vocabulary
      mpVocabulary = new ORBVocabulary();
      Print("Loading ORB Vocabulary. This could take a while...");
      bool bVocLoad = mpVocabulary->loadFromFile(vocabFilename);
      if (!bVocLoad)
      {
         string m("Wrong path to vocabulary. Failed to open at: ");
         m.append(vocabFilename);
         throw exception(m.c_str());
      }
      Print("Vocabulary loaded!");

      //Initialize the Mapper
      mpMapper = new MapperServer(*mpVocabulary, mSensor == MONOCULAR, 1);

      //Create Drawers. These are used by the Viewer
      mpFrameDrawer = new FrameDrawer(settings);
      mpMapDrawer = new MapDrawer(settings, *mpMapper);

      //Initialize the Tracking thread
      //(it will live in the main thread of execution, the one that called this constructor)
      mpTracker = new Tracking(settings, *mpVocabulary, *mpMapper, mpFrameDrawer, mpMapDrawer, mSensor);

      //Initialize the Viewer thread and launch
      if (bUseViewer)
      {
         mpViewer = new Viewer(mpFrameDrawer, mpMapDrawer, mpTracker, *mpMapper, bEmbeddedVideo);
         mptViewer = new thread(&Viewer::Run, mpViewer);
         mpTracker->SetViewer(mpViewer);
      }
   }

   System::~System()
   {
      if (mptViewer)
      {
         mpViewer->RequestFinish();
         mptViewer->join();
         delete mptViewer;
      }
      delete mpViewer;
      delete mpFrameDrawer;
      delete mpMapDrawer;
      delete mpTracker;
      delete mpMapper;
      delete mpVocabulary;
   }

   cv::Mat System::TrackStereo(const cv::Mat &imLeft, const cv::Mat &imRight, const double &timestamp)
   {
      if (mSensor != STEREO)
      {
         throw exception("ERROR: you called TrackStereo but input sensor was not set to STEREO.");
      }

      Frame & f = mpTracker->GrabImageStereo(imLeft, imRight, timestamp);

      // this is disabled until it is needed again
      //unique_lock<mutex> lock2(mMutexState);
      //mTrackingState = mpTracker->mState;
      //mTrackedMapPoints = mpTracker->mCurrentFrame.mvpMapPoints;
      //mTrackedKeyPointsUn = mpTracker->mCurrentFrame.mvKeysUn;
      return f.mTcw.clone();
   }

   cv::Mat System::TrackRGBD(const cv::Mat &im, const cv::Mat &depthmap, const double &timestamp)
   {
      if (mSensor != RGBD)
      {
         throw exception("ERROR: you called TrackRGBD but input sensor was not set to RGBD.");
      }

      Frame & f = mpTracker->GrabImageRGBD(im, depthmap, timestamp);

      // this is disabled until it is needed again
      //unique_lock<mutex> lock2(mMutexState);
      //mTrackingState = mpTracker->mState;
      //mTrackedMapPoints = mpTracker->mCurrentFrame.mvpMapPoints;
      //mTrackedKeyPointsUn = mpTracker->mCurrentFrame.mvKeysUn;
      return f.mTcw.clone();
   }

   cv::Mat System::TrackMonocular(const cv::Mat &im, const double &timestamp)
   {
      if (mSensor != MONOCULAR)
      {
         throw exception("ERROR: you called TrackMonocular but input sensor was not set to Monocular.");
         exit(-1);
      }

      Frame & f = mpTracker->GrabImageMonocular(im, timestamp);

      // this is disabled until it is needed again
      //unique_lock<mutex> lock2(mMutexState);
      //mTrackingState = mpTracker->mState;
      //mTrackedMapPoints = mpTracker->mCurrentFrame.mvpMapPoints;
      //mTrackedKeyPointsUn = mpTracker->mCurrentFrame.mvKeysUn;
      return f.mTcw.clone();
   }

   bool System::MapChanged()
   {
      static int n = 0;
      int curn = mpMapper->GetMap().GetLastBigChangeIdx();
      if (n < curn)
      {
         n = curn;
         return true;
      }
      else
         return false;
   }

   void System::Shutdown()
   {
      if (mptViewer)
      {
         //uncomment to force the viewer to close
         //mpViewer->RequestFinish();

         mptViewer->join();
         delete mptViewer;
         mptViewer = NULL;

         //this causes freeze at shutdown. not sure why it was here.
         //pangolin::BindToContext("ORB-SLAM2-TEAM: Map Viewer");
      }

      Print(to_string(mpTracker->quantityRelocalizations) + " relocalizations");
   }

   void System::SaveTrajectoryTUM(const string &filename)
   {
      stringstream ss;
      ss << endl << "Saving camera trajectory to " << filename << " ...";
      Print(ss);

      if (mSensor == MONOCULAR)
      {
         throw exception("ERROR: SaveTrajectoryTUM cannot be used for monocular.");
      }

      vector<KeyFrame*> vpKFs = mpMapper->GetMap().GetAllKeyFrames();
      sort(vpKFs.begin(), vpKFs.end(), KeyFrame::lId);

      // Transform all keyframes so that the first keyframe is at the origin.
      // After a loop closure the first keyframe might not be at the origin.
      cv::Mat Two = vpKFs[0]->GetPoseInverse();

      ofstream f;
      f.open(filename.c_str());
      f << fixed;

      // Frame pose is stored relative to its reference keyframe (which is optimized by BA and pose graph).
      // We need to get first the keyframe pose and then concatenate the relative transformation.
      // Frames not localized (tracking failure) are not saved.

      // For each frame we have a reference keyframe (lRit), the timestamp (lT) and a flag
      // which is true when tracking failed (lbL).
      list<ORB_SLAM2_TEAM::KeyFrame*>::iterator lRit = mpTracker->mlpReferenceKFs.begin();
      list<double>::iterator lT = mpTracker->mlFrameTimes.begin();
      list<bool>::iterator lbL = mpTracker->mlbLost.begin();
      for (list<cv::Mat>::iterator lit = mpTracker->mlRelativeFramePoses.begin(),
         lend = mpTracker->mlRelativeFramePoses.end();lit != lend;lit++, lRit++, lT++, lbL++)
      {
         if (*lbL)
            continue;

         KeyFrame* pKF = *lRit;

         cv::Mat Trw = cv::Mat::eye(4, 4, CV_32F);

         // If the reference keyframe was culled, traverse the spanning tree to get a suitable keyframe.
         while (pKF->IsBad())
         {
            Trw = Trw * pKF->Tcp;
            pKF = pKF->GetParent();
         }

         Trw = Trw * pKF->GetPose()*Two;

         cv::Mat Tcw = (*lit)*Trw;
         cv::Mat Rwc = Tcw.rowRange(0, 3).colRange(0, 3).t();
         cv::Mat twc = -Rwc * Tcw.rowRange(0, 3).col(3);

         vector<float> q = Converter::toQuaternion(Rwc);

         f << setprecision(6) << *lT << " " << setprecision(9) << twc.at<float>(0) << " " << twc.at<float>(1) << " " << twc.at<float>(2) << " " << q[0] << " " << q[1] << " " << q[2] << " " << q[3] << endl;
      }
      f.close();
      Print("trajectory saved!");
   }


   void System::SaveKeyFrameTrajectoryTUM(const string &filename)
   {
      stringstream ss;
      ss << endl << "Saving keyframe trajectory to " << filename << " ...";
      Print(ss);

      vector<KeyFrame*> vpKFs = mpMapper->GetMap().GetAllKeyFrames();
      sort(vpKFs.begin(), vpKFs.end(), KeyFrame::lId);

      // Transform all keyframes so that the first keyframe is at the origin.
      // After a loop closure the first keyframe might not be at the origin.
      //cv::Mat Two = vpKFs[0]->GetPoseInverse();

      ofstream f;
      f.open(filename.c_str());
      f << fixed;

      for (size_t i = 0; i < vpKFs.size(); i++)
      {
         KeyFrame* pKF = vpKFs[i];

         // pKF->SetPose(pKF->GetPose()*Two);

         if (pKF->IsBad())
            continue;

         cv::Mat R = pKF->GetRotation().t();
         vector<float> q = Converter::toQuaternion(R);
         cv::Mat t = pKF->GetCameraCenter();
         f << setprecision(6) << pKF->timestamp << setprecision(7) << " " << t.at<float>(0) << " " << t.at<float>(1) << " " << t.at<float>(2)
            << " " << q[0] << " " << q[1] << " " << q[2] << " " << q[3] << endl;

      }

      f.close();
      Print("trajectory saved!");
   }

   void System::SaveTrajectoryKITTI(const string &filename)
   {
      stringstream ss;
      ss << endl << "Saving camera trajectory to " << filename << " ...";
      Print(ss);

      if (mSensor == MONOCULAR)
      {
         throw exception("ERROR: SaveTrajectoryKITTI cannot be used for monocular.");
      }

      vector<KeyFrame*> vpKFs = mpMapper->GetMap().GetAllKeyFrames();
      sort(vpKFs.begin(), vpKFs.end(), KeyFrame::lId);

      // Transform all keyframes so that the first keyframe is at the origin.
      // After a loop closure the first keyframe might not be at the origin.
      cv::Mat Two = vpKFs[0]->GetPoseInverse();

      ofstream f;
      f.open(filename.c_str());
      f << fixed;

      // Frame pose is stored relative to its reference keyframe (which is optimized by BA and pose graph).
      // We need to get first the keyframe pose and then concatenate the relative transformation.
      // Frames not localized (tracking failure) are not saved.

      // For each frame we have a reference keyframe (lRit), the timestamp (lT) and a flag
      // which is true when tracking failed (lbL).
      list<ORB_SLAM2_TEAM::KeyFrame*>::iterator lRit = mpTracker->mlpReferenceKFs.begin();
      list<double>::iterator lT = mpTracker->mlFrameTimes.begin();
      for (list<cv::Mat>::iterator lit = mpTracker->mlRelativeFramePoses.begin(), lend = mpTracker->mlRelativeFramePoses.end();lit != lend;lit++, lRit++, lT++)
      {
         ORB_SLAM2_TEAM::KeyFrame* pKF = *lRit;

         cv::Mat Trw = cv::Mat::eye(4, 4, CV_32F);

         while (pKF->IsBad())
         {
            //Print("bad parent");
            Trw = Trw * pKF->Tcp;
            pKF = pKF->GetParent();
         }

         Trw = Trw * pKF->GetPose()*Two;

         cv::Mat Tcw = (*lit)*Trw;
         cv::Mat Rwc = Tcw.rowRange(0, 3).colRange(0, 3).t();
         cv::Mat twc = -Rwc * Tcw.rowRange(0, 3).col(3);

         f << setprecision(9) << Rwc.at<float>(0, 0) << " " << Rwc.at<float>(0, 1) << " " << Rwc.at<float>(0, 2) << " " << twc.at<float>(0) << " " <<
            Rwc.at<float>(1, 0) << " " << Rwc.at<float>(1, 1) << " " << Rwc.at<float>(1, 2) << " " << twc.at<float>(1) << " " <<
            Rwc.at<float>(2, 0) << " " << Rwc.at<float>(2, 1) << " " << Rwc.at<float>(2, 2) << " " << twc.at<float>(2) << endl;
      }
      f.close();
      Print("trajectory saved!");
   }

   int System::GetTrackingState()
   {
      unique_lock<mutex> lock(mMutexState);
      return mTrackingState;
   }

   vector<MapPoint*> System::GetTrackedMapPoints()
   {
      unique_lock<mutex> lock(mMutexState);
      return mTrackedMapPoints;
   }

   vector<cv::KeyPoint> System::GetTrackedKeyPointsUn()
   {
      unique_lock<mutex> lock(mMutexState);
      return mTrackedKeyPointsUn;
   }

   bool System::IsQuitting()
   {
      return mpViewer->CheckFinish();
   }

} //namespace ORB_SLAM