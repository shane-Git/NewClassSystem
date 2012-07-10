#include "StdAfx.h"
#include<iostream>
#include<cstring>
#include<dshow.h>
#include<windows.h>
#include<conio.h>
#pragma comment(lib, "strmiids")
#pragma comment(lib, "quartz")


bool Bstr_Compare(BSTR,BSTR);//Function to compare BSTR strings
void HR_Failed(HRESULT hr);// hr status function
IMoniker* Device_Read(ICreateDevEnum*,IMoniker*,GUID,BSTR);//Device reading function
IBaseFilter* Device_Init(IMoniker*,IBaseFilter*);//Function to initialize Input/Output devices
void Device_Addition(IGraphBuilder*,IBaseFilter*,BSTR);//Function to add device to graph
void Device_Connect(IBaseFilter*,IBaseFilter*);//Function to connect the two devices, in this case input and output
void Run_Graph(IMediaControl*);//Function to run the graph

using namespace std;


int main (void)
{
	HRESULT hr;	// COM result
	IGraphBuilder *pGraph = NULL;// Main graphbuilder pointer
	IMediaControl *pControl = NULL;	// Media Control interface
	ICreateDevEnum *pDeviceEnum = NULL;// System device enumerator
	IBaseFilter *pInputDevice = NULL, *pOutputDevice = NULL;// Input and output filters(devices)
	IMoniker *pDeviceMonik = NULL;// Device moniker
	GUID DEVICE_CLSID ;// GUID i.e.  CLSID_Xxxxxxxxxxx
	BSTR bstrDeviceName= {'\0'};

	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED );// Initialise COM
	if (FAILED(hr))
	{
		HR_Failed(hr);
		return hr;
	}
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,IID_IGraphBuilder, (void**)&pGraph);//Initialize Graph builder
	if (FAILED(hr))
	{
		HR_Failed(hr);
		return hr;
	}
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,IID_ICreateDevEnum, (void **)&pDeviceEnum);//Initialize Device enumerator
	if (FAILED(hr))
	{
		HR_Failed(hr);
		return hr;
	}
	hr = pGraph->QueryInterface(IID_IMediaControl,(void**)&pControl);// Query interface for IMediaControl
	if (FAILED(hr))
	{
		HR_Failed(hr);
		return hr;
	}
/**********************************************************************************/
//Front mic input
	DEVICE_CLSID = CLSID_AudioInputDeviceCategory;// the input device category
	bstrDeviceName = SysAllocString(L"ÏßÂ· (3- USB Multimedia Audio Dev");// device name as seen in Graphedit.exe
	pDeviceMonik = Device_Read(pDeviceEnum,pDeviceMonik,DEVICE_CLSID,bstrDeviceName);//read the required device 
	cout<<pDeviceMonik<<endl;
	pInputDevice = Device_Init(pDeviceMonik,pInputDevice);//Return the device after initializing it
	Device_Addition(pGraph,pInputDevice,bstrDeviceName);//add device to graph
	SysFreeString(bstrDeviceName);
/******************************************************************************/
//Default output device
	DEVICE_CLSID = CLSID_AudioRendererCategory;// the audio renderer device category
	bstrDeviceName = SysAllocString(L"DirectSound: ÑïÉùÆ÷ (3- USB Multimedia Audio Device)");// device name as seen in Graphedit.exe
	cout<<pDeviceMonik<<endl;
	pDeviceMonik = Device_Read(pDeviceEnum,pDeviceMonik,DEVICE_CLSID,bstrDeviceName);//read the required device
	pOutputDevice = Device_Init(pDeviceMonik,pOutputDevice);//Return the device after initializing it
	Device_Addition(pGraph,pOutputDevice,bstrDeviceName);//add device to graph
	SysFreeString(bstrDeviceName);
/******************************************************************************/
//Connect input to output
	Device_Connect(pInputDevice,pOutputDevice);
/*******************************************************************************/
//Now run the graph
	Run_Graph(pControl);

//Loop till you don't close the console window or hit a key!
cout<<"Close the window to exit or hit any key"<<endl;
 while(!_kbhit())
 {
 }
	pControl->Release();//Release control
	pDeviceEnum->Release();//Release Device enumerator
	pGraph->Release();//Release the Graph
}

/************************************************************/
bool Bstr_Compare(BSTR bstrFilter,BSTR bstrDevice)
{
	bool flag = true;
	int strlenFilter = SysStringLen(bstrFilter);//set string length
	int strlenDevice = SysStringLen(bstrDevice);//set string length
	char* chrFilter = (char*)malloc(strlenFilter+1);// allocate memory
	char* chrDevice = (char*)malloc(strlenDevice+1);// allocate memory
	int j = 0;

	if (strlenFilter!=strlenDevice)//if the strings are of not the same length,means they totall different strings
		flag = false;//sety flag to false to indicate "not-same" strings
	else
	{
		for(; j < strlenFilter;j++)//now, copy 1 by 1 each char to chrFilter and chrDevice respectively
		{
			chrFilter[j] = (char)bstrFilter[j];//copy
			chrDevice[j] = (char)bstrDevice[j];//copy
			cout<<j;
			
		}
		chrFilter[strlenFilter] = '\0';//add terminating character
		chrDevice[strlenDevice] = '\0';//add terminating character

		for(j=0; j < strlenFilter;j++)//check loop
		{
			if(chrFilter[j] != chrDevice[j])//check if there are chars that are not samne
				flag = false;//if chars are not same, set flag to false to indicate "not-same" strings
		}
		
		if(flag == true && j == strlenFilter-1)//see if we went through the 'check loop' 
			flag = true;//means strings are same
	}
return flag;
}
void HR_Failed(HRESULT hr)
{
	TCHAR szErr[MAX_ERROR_TEXT_LEN];
        DWORD res = AMGetErrorText(hr, szErr, MAX_ERROR_TEXT_LEN);
        if (res == 0)
        {
            StringCchPrintf(szErr, MAX_ERROR_TEXT_LEN, L"Unknown Error: 0x%2x", hr);
        }
    
		MessageBox(0, szErr, TEXT("Error!"), MB_OK | MB_ICONERROR);
		return;
}


IMoniker* Device_Read(ICreateDevEnum* pDeviceEnum,IMoniker *pDeviceMonik,GUID DEVICE_CLSID,BSTR bstrDeviceName)
{
	HRESULT hr;
	IEnumMoniker *pEnumCat = NULL;// Device enumeration moniker
	VARIANT varName;

	hr = pDeviceEnum->CreateClassEnumerator(DEVICE_CLSID, &pEnumCat, 0);// Enumerate the specified device, distinguished by DEVICE_CLSID

	if (hr == S_OK) 
	{
    ULONG cFetched;
	while (pEnumCat->Next(1, &pDeviceMonik, &cFetched) == S_OK)//Pickup as moniker
	{
        IPropertyBag *pPropBag = NULL;
        hr = pDeviceMonik->BindToStorage(0, 0, IID_IPropertyBag,(void **)&pPropBag);//bind the properties of the moniker
        if (SUCCEEDED(hr))
        {
			VariantInit(&varName);// Initialise the variant data type
			hr = pPropBag->Read(L"FriendlyName", &varName, 0);
      		if (SUCCEEDED(hr))
            {	
				if(Bstr_Compare(varName.bstrVal,bstrDeviceName) == true)//make a comparison
				{	
					wcout<<varName.bstrVal<<" found"<<endl;
					return pDeviceMonik;
				}
			}
			else HR_Failed(hr);
			VariantClear(&varName);//clear the variant data type
			pPropBag->Release();//release the properties
        }
		else HR_Failed(hr);
		pDeviceMonik->Release();//release Device moniker
	}
	pEnumCat->Release();//release category enumerator
	}
	else HR_Failed(hr);
	return NULL;
}

IBaseFilter* Device_Init(IMoniker* pDeviceMonik,IBaseFilter* pDevice)
{
	HRESULT hr;
	hr = pDeviceMonik->BindToObject(NULL, NULL, IID_IBaseFilter,(void**)&pDevice);//Instantiate the device
	if (SUCCEEDED(hr))
	{
		cout<<"Device initiation successful..."<<endl;
	}
	else HR_Failed(hr);

return pDevice;
}

void Device_Addition(IGraphBuilder* pGraph,IBaseFilter* pDevice,BSTR bstrName)
{
	HRESULT hr;
	hr = pGraph->AddFilter(pDevice,bstrName);
	if(SUCCEEDED(hr))
		{
			wcout<<"Addition of "<<bstrName<<" successful..."<<endl;
		}
		else HR_Failed(hr);
}

void Device_Connect(IBaseFilter* pInputDevice,IBaseFilter* pOutputDevice)
{
	HRESULT hr;
	IEnumPins *pInputPin = NULL,*pOutputPin  = NULL;// Pin enumeration
	IPin *pIn = NULL, *pOut = NULL;// Pins

	hr = pInputDevice->EnumPins(&pInputPin);// Enumerate the pin
	if(SUCCEEDED(hr))
	{
		cout<<"Input Pin Enumeration successful..."<<endl;
		hr = pInputDevice->FindPin(L"Capture",&pIn);//Get hold of the pin "Capture", as seen in GraphEdit
		if(SUCCEEDED(hr))
		{
		 cout<<"Capture pin found"<<endl;	
		}
		else HR_Failed(hr);
	
	}
	else HR_Failed(hr);
		
	hr = pOutputDevice->EnumPins(&pOutputPin);//Enumerate the pin
	if(SUCCEEDED(hr))
	{
		cout<<"Output Pin Enumeration successful..."<<endl;
		hr = pOutputDevice->FindPin(L"Audio Input pin (rendered)",&pOut);
		if(SUCCEEDED(hr))
		{
			cout<<"Audio Input Pin (rendered) found"<<endl;
		}
		else HR_Failed(hr);

	}
	else HR_Failed(hr);
	
	hr = pIn->Connect(pOut,NULL);	//Connect the input pin to output pin
	if(SUCCEEDED(hr))
		{
			cout<<"Pin connection successful..."<<endl;
		}
	else HR_Failed(hr);
	
}

void Run_Graph(IMediaControl* pControl)
{
	HRESULT hr;
	hr = pControl->Run();// Now run the graph, i.e. start listening!
	if(SUCCEEDED(hr))
		{
			cout<<"You must be listening to something!!!"<<endl;
		}
	else HR_Failed(hr);
}