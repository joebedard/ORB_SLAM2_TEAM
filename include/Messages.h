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

#ifndef MESSAGES_H
#define MESSAGES_H

#include "Enums.h"
#include "Typedefs.h"

namespace ORB_SLAM2_TEAM
{

   struct GreetRequest
   {
      ServiceId serviceId;
      char message[1];
   };

   struct LoginTrackerRequest
   {
      ServiceId serviceId;
      float pivotCalib[16];
   };

   struct GeneralRequest
   {
      ServiceId serviceId;
      unsigned int trackerId;
   };

   struct GeneralReply
   {
      ReplyCode replyCode;
      char message[1];
   };

   struct LoginTrackerReply
   {
      ReplyCode replyCode;
      unsigned int trackerId;
      id_type firstKeyFrameId;
      unsigned int keyFrameIdSpan;
      id_type firstMapPointId;
      unsigned int mapPointIdSpan;
   };

   struct InsertKeyFrameReply
   {
      ReplyCode replyCode;
      bool inserted;
   };

   struct GeneralMessage
   {
      int subscribeId;
      MessageId messageId;
   };

   struct UpdateTrackerMessage
   {
      int subscribeId;
      MessageId messageId;
      unsigned int trackerId;
   };
}

#endif // MESSAGES_H