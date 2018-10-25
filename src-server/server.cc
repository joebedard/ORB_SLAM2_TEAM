/**
* This file is part of ORB-SLAM2-NET.
*
* Copyright (C) 2018 Joe Bedard <mr dot joe dot bedard at gmail dot com>
* For more information see <https://github.com/joebedard/ORB_SLAM2_NET>
*
* ORB-SLAM2-NET is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2-NET is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2-NET. If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <conio.h>
#include <opencv2/core/core.hpp>
#include <zmq.hpp>
#include <SyncPrint.h>
#include <Sleep.h>

using namespace ORB_SLAM2;

/***
   ORB_SLAM2 Server with shared map for multiple tracking clients.
***/

// logging variables
SyncPrint gOutMain("main: ");
SyncPrint gOutServ("server: ");

// command line parameters
char * gVocabFilename = NULL;
char * gMapperFilename = NULL;

// reply server variables
struct ServerParam
{
   int returnCode;
   std::string * serverAddress;
};
bool gShouldRun = true;

void ParseParams(int paramc, char * paramv[])
{
   if (paramc != 3)
   {
      const char * usage = "Usage: ./server vocabulary_file_and_path mapper_settings_file_and_path";
      std::exception e(usage);
      throw e;
   }
   gVocabFilename = paramv[1];
   gMapperFilename = paramv[2];
}

void VerifySettings(cv::FileStorage & settings, const char * settingsFilePath, std::string & serverAddress, std::string & publisherAddress)
{
   if (!settings.isOpened())
   {
      std::string m("Failed to open settings file at: ");
      m.append(settingsFilePath);
      throw std::exception(m.c_str());
   }

   serverAddress.append(settings["Server.Address"]);
   if (0 == serverAddress.length())
      throw std::exception("Server.Address property is not set or value is not in quotes.");

   publisherAddress.append(settings["Publisher.Address"]);
   if (0 == publisherAddress.length())
      throw std::exception("Publisher.Address property is not set or value is not in quotes.");
}

void RunServer(void * param) try
{
   ServerParam * serverParam = (ServerParam *)param;

   zmq::context_t context(1);
   zmq::socket_t socket(context, ZMQ_REP);
   socket.bind(*serverParam->serverAddress);

   zmq::message_t request;
   while (gShouldRun) {
      // check for request from client
      if (socket.recv(&request, ZMQ_NOBLOCK))
      {
         if (strncmp((const char *)request.data(), "Hello", 5))
         {
            std::cout << "Received Hello" << std::endl;

            //  TODO - dispatch request
            sleep(1000);

            // send reply back to client
            zmq::message_t reply(5);
            memcpy(reply.data(), "World", 5);
            socket.send(reply);
         }
      }
      else
      {
         sleep(1000);
      }
   }

   serverParam->returnCode = EXIT_SUCCESS;
}
catch (const std::exception& e)
{
   gOutServ.Print(e.what());
   ServerParam * serverParam = (ServerParam *)param;
   serverParam->returnCode = EXIT_FAILURE;
}
catch (...)
{
   gOutServ.Print("an exception was not caught");
   ServerParam * serverParam = (ServerParam *)param;
   serverParam->returnCode = EXIT_FAILURE;
}


int main(int argc, char * argv[]) try
{
   ParseParams(argc, argv);

   cv::FileStorage mapperSettings(gMapperFilename, cv::FileStorage::READ);
   std::string serverAddress, publisherAddress;
   VerifySettings(mapperSettings, gMapperFilename, serverAddress, publisherAddress);

   ServerParam param;
   param.serverAddress = &serverAddress;
   thread serverThread(RunServer, &param);

   // Output welcome message
   stringstream ss1;
   ss1 << endl;
   ss1 << "ORB-SLAM2-NET Server" << endl;
   ss1 << "Copyright (C) 2014-2016 Raul Mur-Artal, University of Zaragoza" << endl;
   ss1 << "Copyright (C) 2018 Joe Bedard" << endl;
   ss1 << "This program comes with ABSOLUTELY NO WARRANTY;" << endl;
   ss1 << "This is free software, and you are welcome to redistribute it" << endl;
   ss1 << "under certain conditions. See LICENSE.txt." << endl << endl;
   ss1 << "Server.Address=" << serverAddress << endl;
   ss1 << "Publisher.Address=" << publisherAddress << endl;
   ss1 << "Press X to exit." << endl << endl;
   gOutMain.Print(NULL, ss1);

   int key = 0;
   while (key != 'x' && key != 'X' && key != 27) // 27 == Esc
   {
      sleep(1000);
      key = getch();
   }
   gOutMain.Print(NULL, "Shutting down server...");
   gShouldRun = false; //signal threads to stop
   serverThread.join();

   return EXIT_SUCCESS;
}
catch (const std::exception& e)
{
   gOutMain.Print(e.what());
   return EXIT_FAILURE;
}
catch (...)
{
   gOutMain.Print("an exception was not caught");
   return EXIT_FAILURE;
}
