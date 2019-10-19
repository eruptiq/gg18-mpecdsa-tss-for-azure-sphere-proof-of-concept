#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>

#include "network.h"
#include <applibs/storage.h>
#include <applibs/log.h>
#include <errno.h>


/// <summary>
///     Logs a cURL error.
/// </summary>
/// <param name="message">The message to print</param>
/// <param name="curlErrCode">The cURL error code to describe</param>
static void LogCurlError(const char* message, int curlErrCode)
{
	Log_Debug(message);
	Log_Debug(" (curl err=%d, '%s')\n", curlErrCode, curl_easy_strerror(curlErrCode));
}

void LogErrno(const char* message, ...)
{
	int currentErrno = errno;
	va_list argptr;
	va_start(argptr, message);
	Log_DebugVarArgs(message, argptr);
	va_end(argptr);
	Log_Debug(" (errno=%d, '%s').\n", currentErrno, strerror(currentErrno));
}

struct url_data {
	size_t size;
	char* data;
};

size_t write_data(void* ptr, size_t size, size_t nmemb, struct url_data* data) {
	size_t index = data->size;
	size_t n = (size * nmemb);
	char* tmp;

	data->size += (size * nmemb);

#ifdef DEBUG
	fprintf(stderr, "data at %p size=%ld nmemb=%ld\n", ptr, size, nmemb);
#endif
	tmp = realloc(data->data, data->size + 1); /* +1 for '\0' */

	if (tmp) {
		data->data = tmp;
	}
	else {
		if (data->data) {
			free(data->data);
		}
		fprintf(stderr, "Failed to allocate memory.\n");
		return 0;
	}

	memcpy((data->data + index), ptr, n);
	data->data[data->size] = '\0';

	return size * nmemb;
}

char* curl_post(char* url, char* body) {
	CURL* curlHandle;
	CURLcode res = 0;
	// Certificates bundle path storage.
	char* certificatePath = NULL;

	struct url_data data;
	data.size = 0;
	data.data = malloc(4096); /* reasonable size initial buffer */
	if (NULL == data.data) {
		fprintf(stderr, "Failed to allocate memory.\n");
		return NULL;
	}

	data.data[0] = '\0';

	curlHandle = curl_easy_init();
	if (curlHandle) {
		//curl_easy_setopt(curlHandle, CURLOPT_URL, url);
		//curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data);
		//curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &data);
		//res = curl_easy_perform(curlHandle);
		//if (res != CURLE_OK) {
		//	fprintf(stderr, "curl_easy_perform() failed: %s\n",
		//		curl_easy_strerror(res));
		//}

		//curl_easy_cleanup(curlHandle);
		/* First set the URL that is about to receive our POST. This URL can
	    just as well be a https:// URL if that is what should receive the
	    data. */

		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &data);

		curl_easy_setopt(curlHandle, CURLOPT_POST, 1L);

		//curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1L);

		// Specify URL to download.
		// Important: any change in the domain name must be reflected in the AllowedConnections
		// capability in app_manifest.json.
		if ((res = curl_easy_setopt(curlHandle, CURLOPT_URL, url)) != CURLE_OK) {
			LogCurlError("curl_easy_setopt CURLOPT_URL", res);
			goto errorLabel;
		}

		// Set output level to verbose.
		if ((res = curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1L)) != CURLE_OK) {
			LogCurlError("curl_easy_setopt CURLOPT_VERBOSE", res);
			goto errorLabel;
		}

		// Get the full path to the certificate file used to authenticate the HTTPS server identity.
		// The infura_full.pem file is the certificate that is used to verify the
		// server identity.
		certificatePath = Storage_GetAbsolutePathInImagePackage("certs/infura_full.pem");
		if (certificatePath == NULL) {
			Log_Debug("The certificate path could not be resolved: errno=%d (%s)\n", errno,
				strerror(errno));
			goto errorLabel;
		}

		// Set the path for the certificate file that cURL uses to validate the server certificate.
		if ((res = curl_easy_setopt(curlHandle, CURLOPT_CAINFO, certificatePath)) != CURLE_OK) {
			LogCurlError("curl_easy_setopt CURLOPT_CAINFO", res);
			goto errorLabel;
		}

		// Let cURL follow any HTTP 3xx redirects.
		// Important: any redirection to different domain names requires that domain name to be added to
		// app_manifest.json.
		if ((res = curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L)) != CURLE_OK) {
			LogCurlError("curl_easy_setopt CURLOPT_FOLLOWLOCATION", res);
			goto errorLabel;
		}

		//// Set SSL verify host.
		//if ((res = curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYHOST, 0L)) != CURLE_OK) {
		//	LogCurlError("curl_easy_setopt CURLOPT_SSL_VERIFYHOST", res);
		//	goto errorLabel;
		//}

		//// Set SSL verify peer.
		//if ((res = curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYPEER, 0L)) != CURLE_OK) {
		//	LogCurlError("curl_easy_setopt CURLOPT_SSL_VERIFYHOST", res);
		//	goto errorLabel;
		//}

		struct curl_slist* hs = NULL;
		hs = curl_slist_append(hs, "Content-Type: application/json");
		curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, hs);

		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, body);

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curlHandle);
		/* Check for errors */
		if (res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));

		/* always cleanup */
		free(certificatePath);

		curl_easy_cleanup(curlHandle);
	}
	// Clean up cURL library's resources.
	curl_global_cleanup();
	return data.data;

errorLabel:
	// Clean up allocated memory.
	free(data.data);
	free(certificatePath);
	curl_easy_cleanup(curlHandle);
	curl_global_cleanup();

	return NULL;
}