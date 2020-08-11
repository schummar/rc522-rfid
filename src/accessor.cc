#include <node_api.h>
#include <unistd.h>
#include <stdio.h>
#include <list>
#include <assert.h>
// #include "rfid.h"
// #include "rc522.h"
// #include "bcm2835.h"

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

// void RunCallback(const FunctionCallbackInfo<Value> &args)
// {
// Isolate *isolate = Isolate::GetCurrent();
// Local<Context> context = isolate->GetCurrentContext();
// HandleScope scope(isolate);

// Local<Object> options = Local<Object>::Cast(args[0]);
// double delay = options->Get(context, String::NewFromUtf8(isolate, "delay")).ToLocalChecked().As<Number>()->Value();
// Local<Function> callback = Local<Function>::Cast(args[1]);

// for (;;)
// {
// 	InitRc522();
// 	statusRfidReader = find_tag(&CType);

// 	if (statusRfidReader == TAG_NOTAG)
// 	{
// 		foundTag = false;
// 	}
// 	else if (!(statusRfidReader != TAG_OK && statusRfidReader != TAG_COLLISION) && select_tag_sn(serialNumber, &serialNumberLength) == TAG_OK)
// 	{
// 		foundTag = true;
// 		p = uid;
// 		for (loopCounter = 0; loopCounter < serialNumberLength; loopCounter++)
// 		{
// 			sprintf(p, "%02x", serialNumber[loopCounter]);
// 			p += 2;
// 		}
// 	}

// 	if (foundTag != lastFoundTag || strcmp(uid, lastUid) != 0)
// 	{
// 		Local<Value> argv[1];
// 		if (foundTag)
// 		{
// 			argv[0] = String::NewFromUtf8(isolate, &uid[0]);
// 		}
// 		else
// 		{
// 			argv[0] = Null(isolate);
// 		}
// 		callback->Call(context, Null(isolate), 1, argv).ToLocalChecked();
// 	}

// 	lastFoundTag = foundTag;
// 	strcpy(lastUid, uid);
// 	usleep((long)delay * 1000);
// }

// bcm2835_spi_end();
// bcm2835_close();
// }

// napi_value Method(napi_env env, napi_callback_info info)
// {
// 	size_t argc = 1;
// 	napi_value argv[1];
// 	napi_status status;
// 	status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
// 	int number;
// 	status = napi_get_value_int32(env, argv[0], &number);
// 	if (status != napi_ok)
// 	{
// 		napi_throw_error(env, NULL, "Invalid number was passed as argument");
// 	}

// 	printf("Hello %d\n", number);
// 	return NULL;
// }

// napi_value Init(napi_env env, napi_value exports)
// {
// 	// initRfidReader();
// 	// NODE_SET_METHOD(module, "exports", RunCallback);
// 	napi_value method;
// 	napi_status status;
// 	status = napi_create_function(env, "exports", NAPI_AUTO_LENGTH, Method, NULL, &method);
// 	if (status != napi_ok)
// 		return NULL;
// 	return method;
// }

// uint8_t initRfidReader(void)
// {
// 	uint16_t sp;

// 	sp = (uint16_t)(250000L / DEFAULT_SPI_SPEED);
// 	if (!bcm2835_init())
// 	{
// 		return 1;
// 	}

// 	bcm2835_spi_begin();
// 	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST); // The default
// 	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);				 // The default
// 	bcm2835_spi_setClockDivider(sp);						 // The default
// 	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);				 // The default
// 	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); // the default
// 	return 0;
// }

// NAPI_MODULE(rc522, Init)

struct Data
{
	long delay;
	napi_async_work work;
	napi_threadsafe_function callback;
};

void jsCallbackProcessor(napi_env env, napi_value js_cb,
						 void *context, void *data)
{
	if (env != NULL)
	{
		char *uid = (char *)data;
		printf("uid: %s\n", uid);
		napi_value result, undefined;
		assert(napi_create_string_utf8(env, "foo", NAPI_AUTO_LENGTH, &result));
		assert(napi_get_undefined(env, &undefined));
		assert(napi_call_function(env, undefined, js_cb, 1, &result, NULL));
		delete uid;
	}
}

void execute(napi_env env, void *dataIn)
{
	Data *data = (Data *)dataIn;

	assert(napi_acquire_threadsafe_function(data->callback) == napi_ok);

	for (int i = 0; i < 10; i++)
	{
		usleep(data->delay * 1000);
		char *uid = new char[100];
		sprintf(uid, "%d,", i);
		assert(napi_call_threadsafe_function(data->callback, uid, napi_tsfn_nonblocking) == napi_ok);
	}

	assert(napi_release_threadsafe_function(data->callback, napi_tsfn_release) == napi_ok);
}

void onComplete(napi_env env, napi_status status, void *dataIn)
{
	Data *data = (Data *)dataIn;
	assert(napi_release_threadsafe_function(data->callback, napi_tsfn_release) == napi_ok);
	assert(napi_delete_async_work(env, data->work) == napi_ok);
	delete data;
}

napi_value start(napi_env env, napi_callback_info info)
{
	size_t argc = 2;
	napi_value args[2];
	assert(napi_get_cb_info(env, info, &argc, args, NULL, NULL) == napi_ok);
	napi_value options;
	napi_value delay;
	assert(napi_get_named_property(env, args[0], "delay", &delay) == napi_ok);
	napi_value jsCallback = args[1]; // Second param, the JS callback function

	// Specify a name to describe this asynchronous operation.
	napi_value workName;
	assert(napi_create_string_utf8(env, "Work", NAPI_AUTO_LENGTH, &workName) == napi_ok);

	// Create a thread-safe N-API callback function correspond to the C/C++ callback function
	Data *data = new Data;
	assert(napi_get_value_int64(env, delay, &data->delay) == napi_ok);
	assert(napi_create_threadsafe_function(env, jsCallback, NULL, workName, 0, 1, NULL, NULL, NULL, jsCallbackProcessor, &data->callback) == napi_ok);
	assert(napi_create_async_work(env, NULL, workName, execute, onComplete, data, &data->work) == napi_ok);
	assert(napi_queue_async_work(env, data->work) == napi_ok);

	// This causes `undefined` to be returned to JavaScript.
	return NULL;
}

napi_value Init(napi_env env, napi_value exports)
{
	// initRfidReader();
	// NODE_SET_METHOD(module, "exports", RunCallback);
	napi_value method;
	napi_status status;
	status = napi_create_function(env, "exports", NAPI_AUTO_LENGTH, start, NULL, &method);
	if (status != napi_ok)
		return NULL;
	return method;
}

NAPI_MODULE(rc522, Init)