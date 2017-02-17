// BGaze.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <eyex\EyeX.h>
#include <assert.h>
#pragma comment (lib, "Tobii.EyeX.Client.lib")

#define A_SIZE 20 // size of array for avg position
#define LOOKBACK 15 // how far to look back to see if there was a double blink
#define LONG_BLINK 275 // approx time it takes for a person to blink intentionaly, measured in millisecond
#define SHORT_BLINK 100 // approx time it takes for a person to blink, measured in milliseconds

#define KEY_ESC 27
#define KEY_SPACE 32

//stores the delta Times
static int deltaTimes[LOOKBACK];

// array to store x and y coord used for finding avg position
static int Xpos[A_SIZE];
static int Ypos[A_SIZE];

static TX_HANDLE g_hGlobalInteractorSnapshot = TX_EMPTY_HANDLE; // global variable for snapshot
static char c = NULL; // last entered char
static int count = 0; // amount of times OnGazeDataEvent has been called
static int prevTime = 0;

bool InitializeGlobalInteractorSnapshot(TX_CONTEXTHANDLE hContext);
void TX_CALLCONVENTION OnEngineConnectionStateChanged(TX_CONNECTIONSTATE connectionState, TX_USERPARAM userParam);
void OnGazeDataEvent(TX_HANDLE hGazeDataBehavior);
void TX_CALLCONVENTION OnSnapshotCommitted(TX_CONSTHANDLE hAsyncData, TX_USERPARAM param);
void TX_CALLCONVENTION HandleEvent(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam);
int avg(int ary[]);
void leftClick();

// initalize EyeX and create context handle
int _tmain(int argc, _TCHAR* argv[])
{
	TX_CONTEXTHANDLE hContext = TX_EMPTY_HANDLE; // initalize EyeX library and create interaction context
	TX_TICKET hConnectionStateChangedTicket = TX_INVALID_TICKET; // connection state change handler for snapshot - recieve callbacks whenever connection state changes
	TX_TICKET hEventHandlerTicket = TX_INVALID_TICKET;

	bool success;

	// intialize and enable context that is our link to EyeXEngine
	success = txInitializeEyeX(TX_EYEXCOMPONENTOVERRIDEFLAG_NONE, NULL, NULL, NULL, NULL) == TX_RESULT_OK;
	success &= txCreateContext(&hContext, TX_FALSE) == TX_RESULT_OK;
	success &= txEnableConnection(hContext) == TX_RESULT_OK;
	success &= InitializeGlobalInteractorSnapshot(hContext); // create global snapshot
	success &= txRegisterConnectionStateChangedHandler(hContext, &hEventHandlerTicket, OnEngineConnectionStateChanged, NULL) == TX_RESULT_OK; // for connection state change handler
	success &= txRegisterEventHandler(hContext, &hEventHandlerTicket, HandleEvent, NULL) == TX_RESULT_OK; // to register event handler
	success &= txEnableConnection(hContext) == TX_RESULT_OK; // connect to EyeX Engine

	if (success) {
		printf("Initialization was successful.\n");
	}
	else {
		printf("Initialization failed.\n");
	}
	printf("Press any key to exit...\n");

	// exit program when ESC is pressed
	while (c != KEY_ESC)
	{
		c = _getch();
	}
	printf("Exiting.\n");

	// disable and delete the context
	txDisableConnection(hContext);
	txReleaseObject(&g_hGlobalInteractorSnapshot);
	txShutdownContext(hContext, TX_CLEANUPTIMEOUT_DEFAULT, TX_FALSE);
	txReleaseContext(&hContext);

	if (success) return 0;
	else return 1;
}

// API that creates global snapshot along with global interactor
bool InitializeGlobalInteractorSnapshot(TX_CONTEXTHANDLE hContext)
{
	TX_HANDLE hInteractor = TX_EMPTY_HANDLE;
	TX_GAZEPOINTDATAPARAMS params = { TX_GAZEPOINTDATAMODE_LIGHTLYFILTERED };
	bool success;

	success = txCreateGlobalInteractorSnapshot(
		hContext,
		"InteractorId",
		&g_hGlobalInteractorSnapshot,
		&hInteractor) == TX_RESULT_OK;
	success &= txCreateGazePointDataBehavior(hInteractor, &params) == TX_RESULT_OK;
	txReleaseObject(&hInteractor);
	return success;
}

// check to see if connection state has changed and ONLY commit global interaction snapshot if so
void TX_CALLCONVENTION OnEngineConnectionStateChanged(TX_CONNECTIONSTATE connectionState, TX_USERPARAM userParam)
{
	switch (connectionState) {
	case TX_CONNECTIONSTATE_CONNECTED: {
		bool success;
		printf("The connection state is now CONNECTED (We are connected to the EyeX Engine)\n");
		// commit the snapshot with the global interactor as soon as the connection to the engine is established.
		// (it cannot be done earlier because committing means "send to the engine".)
		success = txCommitSnapshotAsync(g_hGlobalInteractorSnapshot, OnSnapshotCommitted, NULL) == TX_RESULT_OK;
		if (!success) {
			printf("Failed to initialize the data stream.\n");
		}
		else
		{
			printf("Waiting for gaze data to start streaming...\n");
		}
	}
		break;
	}
}

// handles an event from the Gaze Point data stream
void OnGazeDataEvent(TX_HANDLE hGazeDataBehavior)
{
	TX_GAZEPOINTDATAEVENTPARAMS eventParams;
	if (txGetGazePointDataEventParams(hGazeDataBehavior, &eventParams) == TX_RESULT_OK) {
		int deltaTime = eventParams.Timestamp - prevTime; // difference between last time and current time
		//printf("%d \n", deltaTime);

		SetCursorPos(avg(Xpos), avg(Ypos));
		// set cursor to (x,y) if user has blinked or if the spacebar is pressed
		if ((deltaTime > LONG_BLINK || c == KEY_SPACE) && count != 0) {
			//SetCursorPos(avg(Xpos), avg(Ypos));
			printf("%d %d \n", avg(Xpos), avg(Ypos));
			leftClick();
			c = NULL;
		}
		// double click
		//else if (deltaTime > SHORT_BLINK) {
		//	for (int i = 0; i < LOOKBACK; i++)
		//	{
		//		if (deltaTimes[i] > SHORT_BLINK)
		//		{
		//			leftClick();
		//			leftClick();
		//		}
		//	}
		//}

		// add new values into the position arrays
		deltaTimes[count % LOOKBACK] = deltaTime;
		Xpos[count % A_SIZE] = eventParams.X;
		Ypos[count % A_SIZE] = eventParams.Y;
		prevTime = eventParams.Timestamp;
		count++;
	}
	else {
		printf("Failed to interpret gaze data event packet.\n");
	}
}

void leftClick()
{
	INPUT	Input = { 0 };
	Input.type = INPUT_MOUSE;

	Input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	::SendInput(1, &Input, sizeof(INPUT));

	::ZeroMemory(&Input, sizeof(INPUT));

	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	::SendInput(1, &Input, sizeof(INPUT));
}

// get average of values (position values) stored in array
int avg(int ary[])
{
	int avg = 0;
	for (int i = 0; i < A_SIZE; i++)
	{
		avg += ary[i];
	}
	avg = avg / A_SIZE;
	return avg;
}

// if snapshot is committed
void TX_CALLCONVENTION OnSnapshotCommitted(TX_CONSTHANDLE hAsyncData, TX_USERPARAM param)
{
	// check the result code using an assertion.
	// this will catch validation errors and runtime errors in debug builds. in release builds it won't do anything.

	TX_RESULT result = TX_RESULT_UNKNOWN;
	txGetAsyncDataResultCode(hAsyncData, &result);
	assert(result == TX_RESULT_OK || result == TX_RESULT_CANCELLED);
}

// for event handler
void TX_CALLCONVENTION HandleEvent(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam)
{
	TX_HANDLE hEvent = TX_EMPTY_HANDLE;
	TX_HANDLE hBehavior = TX_EMPTY_HANDLE;

	txGetAsyncDataContent(hAsyncData, &hEvent);

	// NOTE. Uncomment the following line of code to view the event object. The same function can be used with any interaction object.
	//OutputDebugStringA(txDebugObject(hEvent));

	if (txGetEventBehavior(hEvent, &hBehavior, TX_BEHAVIORTYPE_GAZEPOINTDATA) == TX_RESULT_OK) {
		OnGazeDataEvent(hBehavior);
		txReleaseObject(&hBehavior);
	}

	// NOTE since this is a very simple application with a single interactor and a single data stream,
	// our event handling code can be very simple too. A more complex application would typically have to
	// check for multiple behaviors and route events based on interactor IDs.

	txReleaseObject(&hEvent);
}
