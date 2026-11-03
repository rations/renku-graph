/*
 * Copyright 2026, rations.
 * Distributed under the terms of the MIT License.
 * SPDX-License-Identifier: MIT
 */
#ifndef MEDIA_WORKER_H
#define MEDIA_WORKER_H


#include <Looper.h>
#include <MediaDefs.h>
#include <MediaNode.h>
#include <Messenger.h>

#include <vector>


class BMediaRoster;
class BMessageRunner;


/*!	Owns every BMediaRoster call of the app. A node that does not answer can
	block a roster call for a long time; keeping the calls on this looper
	keeps the windows responsive.
*/
class MediaWorker : public BLooper {
public:
								MediaWorker(const BMessenger& target);
	virtual						~MediaWorker();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_Start();
			bool				_EnsureRoster();
			void				_ScheduleScan(bigtime_t delay);
			void				_Scan();
			void				_Connect(BMessage* message);
			void				_Disconnect(BMessage* message);
			void				_QueryDevices(BMessage* message);
			void				_SetParameter(BMessage* message);
			void				_SetDefault(BMessage* message);
			void				_Restart();
			void				_Status(const char* format, ...)
									__attribute__((format(printf, 2, 3)));

			status_t			_GetInputs(const media_node& node,
									std::vector<media_input>& inputs);
			status_t			_GetOutputs(const media_node& node,
									std::vector<media_output>& outputs);
			status_t			_GetLiveNodes(
									std::vector<live_node_info>& nodes);

			BMessenger			fTarget;
			BMediaRoster*		fRoster;
			BMessageRunner*		fScanRunner;
			bool				fWatching;
			bool				fWatchingServer;
};


#endif	// MEDIA_WORKER_H
