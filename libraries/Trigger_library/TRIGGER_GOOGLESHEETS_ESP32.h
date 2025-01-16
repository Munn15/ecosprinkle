#ifndef __TRIGGER_GOOGLESHEETS_H__
#define __TRIGGER_GOOGLESHEETS_H__

#include <stdarg.h>
#include <string.h>
#include <HTTPClient.h>

void trigsheetDataToSheet(int num,...);
void trigsheetSendData();
void trigsheetInit(char array_2d[10][20], String sheets_gas_id, int param_size);
void trigsheetFloatToStr();

char _trigsheetColNames[ ][20]={"","","","","","","","","",""};;
double _trigsheetArgsValues[100];
char _trigsheetColValues[10][10];

String _trigsheetGASid;
int _trigsheetCount;

void trigsheetInit(char test[ ][20], String sheets_gas_id, int param_size) {
	_trigsheetGASid = sheets_gas_id;
	_trigsheetCount = param_size;
	
	for (int i = 0; i < _trigsheetCount; i++)
		for (int j = 0; j < 20; j++)
			_trigsheetColNames[i][j] = test[i][j];
}

void trigsheetDataToSheet(int num,...) {
	va_list lst;
	va_start(lst,num);

	for (int i=0;i<num;i++)
		_trigsheetArgsValues[i]= va_arg(lst,double);

	va_end(lst);	
	
	trigsheetFloatToStr();
	trigsheetSendData();
}

void trigsheetFloatToStr() {
  for (int j=0;j<_trigsheetCount;j++)
  	sprintf(_trigsheetColValues[j], "%.02f", _trigsheetArgsValues[j]);
}

void trigsheetSendData() {
	HTTPClient http;
	http.setTimeout(15000);
	String url = "https://script.google.com/macros/s/" + _trigsheetGASid + "/exec?";

	int i=0;
	while (i < _trigsheetCount) {
		if(i==0) url = url + _trigsheetColNames[i] + "=" + _trigsheetColValues[i];
		else url = url + "&" + _trigsheetColNames[i] + "=" + _trigsheetColValues[i];
		i++; 
	}
  
	Serial.print("post to: ");
	Serial.println(url);

	http.begin(url.c_str());
	http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET();
	Serial.print("http code: ");
    Serial.println(httpCode);

	String payload;
    if (httpCode > 0) {
        payload = http.getString();
        Serial.println("payload: "+payload);    
    }

    http.end();
}

#endif
