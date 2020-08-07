#include <node.h>
#include <v8.h>
#include <unistd.h>
#include "rfid.h"
#include "rc522.h"
#include "bcm2835.h"

#define DEFAULT_SPI_SPEED 5000L

uint8_t initRfidReader(void);

char statusRfidReader;
uint16_t CType = 0;
uint8_t serialNumber[10];
uint8_t serialNumberLength = 0;
bool foundTag = false;
bool lastFoundTag = false;
char uid[23];
char lastUid[23];
char *p;
int loopCounter;

using namespace v8;

void RunCallback(const FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = Isolate::GetCurrent();
	Local<Context> context = isolate->GetCurrentContext();
	HandleScope scope(isolate);

	Local<Object> options = Local<Object>::Cast(args[0]);
	double delay = options->Get(context, String::NewFromUtf8(isolate, "delay")).ToLocalChecked().As<Number>()->Value();
	Local<Function> callback = Local<Function>::Cast(args[1]);

	for (;;)
	{
		InitRc522();
		statusRfidReader = find_tag(&CType);

		if (statusRfidReader == TAG_NOTAG)
		{
			foundTag = false;
		}
		else if (!(statusRfidReader != TAG_OK && statusRfidReader != TAG_COLLISION) && select_tag_sn(serialNumber, &serialNumberLength) == TAG_OK)
		{
			foundTag = true;
			p = uid;
			for (loopCounter = 0; loopCounter < serialNumberLength; loopCounter++)
			{
				sprintf(p, "%02x", serialNumber[loopCounter]);
				p += 2;
			}
		}

		if (foundTag != lastFoundTag || strcmp(uid, lastUid) != 0)
		{
			Local<Value> argv[1];
			if (foundTag)
			{
				argv[0] = String::NewFromUtf8(isolate, &uid[0]);
			}
			else
			{
				argv[0] = Null(isolate);
			}
			callback->Call(context, Null(isolate), 1, argv).ToLocalChecked();
		}

		lastFoundTag = foundTag;
		strcpy(lastUid, uid);
		usleep((long)delay * 1000);
	}

	bcm2835_spi_end();
	bcm2835_close();
}

void Init(Handle<Object> exports, Handle<Object> module)
{
	initRfidReader();
	NODE_SET_METHOD(module, "exports", RunCallback);
}

uint8_t initRfidReader(void)
{
	uint16_t sp;

	sp = (uint16_t)(250000L / DEFAULT_SPI_SPEED);
	if (!bcm2835_init())
	{
		return 1;
	}

	bcm2835_spi_begin();
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST); // The default
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);				 // The default
	bcm2835_spi_setClockDivider(sp);						 // The default
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);				 // The default
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); // the default
	return 0;
}

NODE_MODULE(rc522, Init)