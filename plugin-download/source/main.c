#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <3ds.h>

Result http_download(const char *url)
{
	Result ret=0;
	httpcContext context;
	char *newurl=NULL;
	u8* framebuf_top;
	u32 statuscode=0;
	u32 contentsize=0, readsize=0, size=0;
	u8 *buf, *lastbuf;

	printf("Downloading your plugin...\n");
	gfxFlushBuffers();

	do {
		ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
		//printf("return from httpcOpenContext: %"PRId32"\n",ret);
		gfxFlushBuffers();

		// This disables SSL cert verification, so https:// will be usable
		ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
		//printf("return from httpcSetSSLOpt: %"PRId32"\n",ret);
		gfxFlushBuffers();

		// Enable Keep-Alive connections (on by default, pending ctrulib merge)
		// ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
		// printf("return from httpcSetKeepAlive: %"PRId32"\n",ret);
		// gfxFlushBuffers();

		// Set a User-Agent header so websites can identify your application
		ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");
		//printf("return from httpcAddRequestHeaderField: %"PRId32"\n",ret);
		gfxFlushBuffers();

		// Tell the server we can support Keep-Alive connections.
		// This will delay connection teardown momentarily (typically 5s)
		// in case there is another request made to the same server.
		ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");
		//printf("return from httpcAddRequestHeaderField: %"PRId32"\n",ret);
		gfxFlushBuffers();

		ret = httpcBeginRequest(&context);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		ret = httpcGetResponseStatusCode(&context, &statuscode);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			return ret;
		}

		if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
			if(newurl==NULL) newurl = malloc(0x1000); // One 4K page for new URL
			if (newurl==NULL){
				httpcCloseContext(&context);
				return -1;
			}
			ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
			url = newurl; // Change pointer to the url that we just learned
			//printf("redirecting to url: %s\n",url);
			httpcCloseContext(&context); // Close this context before we try the next
		}
	} while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

	if(statuscode!=200){
		//printf("URL returned status: %"PRId32"\n", statuscode);
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -2;
	}

	// This relies on an optional Content-Length header and may be 0
	ret=httpcGetDownloadSizeState(&context, NULL, &contentsize);
	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return ret;
	}

	//printf("reported size: %"PRId32"\n",contentsize);
	gfxFlushBuffers();

	// Start with a single page buffer
	buf = (u8*)malloc(0x1000);
	if(buf==NULL){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	do {
		// This download loop resizes the buffer as data is read.
		ret = httpcDownloadData(&context, buf+size, 0x1000, &readsize);
		size += readsize; 
		if (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING){
				lastbuf = buf; // Save the old pointer, in case realloc() fails.
				buf = realloc(buf, size + 0x1000);
				if(buf==NULL){ 
					httpcCloseContext(&context);
					free(lastbuf);
					if(newurl!=NULL) free(newurl);
					return -1;
				}
			}
	} while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);	

	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		free(buf);
		return -1;
	}

	// Resize the buffer back down to our actual final size
	lastbuf = buf;
	buf = realloc(buf, size);
	if(buf==NULL){ // realloc() failed.
		httpcCloseContext(&context);
		free(lastbuf);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	//printf("downloaded size: %"PRId32"\n",size);
	gfxFlushBuffers();

	struct stat st = {0};

	FILE *usa;
	FILE *eur;
	FILE *jap;

	if (stat("sdmc:/plugin/0004000000086200", &st) == -1) 
	{
    	mkdir("sdmc:/plugin/0004000000086200", 0700);
	}
	if (stat("sdmc:/plugin/0004000000086300", &st) == -1) 
	{
    	mkdir("sdmc:/plugin/0004000000086300", 0700);
	}
	if (stat("sdmc:/plugin/0004000000086400", &st) == -1) 
	{
    	mkdir("sdmc:/plugin/0004000000086400", 0700);
	}

	//delete any existing plugins in the USA, EUR or JAP directory
	remove("sdmc:/plugin/0004000000086300/ACNL_Multi.plg");
	remove("sdmc:/plugin/0004000000086200/ACNL_Multi.plg");
	remove("sdmc:/plugin/0004000000086400/ACNL_Multi.plg");
	remove("sdmc:/plugin/0004000000086300/ACNL_Multi_USA.plg");
	remove("sdmc:/plugin/0004000000086200/ACNL_Multi_JAP.plg");
	remove("sdmc:/plugin/0004000000086400/ACNL_Multi_EUR.plg");
	usa = fopen("sdmc:/plugin/0004000000086300/ACNL_MULTI.plg", "w+");
	fwrite(buf , 1 , size , usa );
	fclose(usa);
	eur = fopen("sdmc:/plugin/0004000000086400/ACNL_MULTI.plg", "w+");
	fwrite(buf , 1 , size , eur );
	fclose(eur);
	jap = fopen("sdmc:/plugin/0004000000086200/ACNL_MULTI.plg", "w+");
	fwrite(buf , 1 , size , jap );
	fclose(jap);

	printf("Done!\n");




	return 0;
}

int main()
{
	Result ret=0;
	gfxInitDefault();
	httpcInit(0); // Buffer size when POST/PUT.

	consoleInit(GFX_TOP,NULL);

	printf("---ACNL Multi NTR Plugin Downloader V1.1---\n");
	printf("Press A to download the latest version \n");
	printf("Press B to download 3.0 beta (for people without\nthe Amiibo update) \n");
	printf("Press Start to exit.");


	gfxFlushBuffers();


	while (aptMainLoop())
	{
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();

		if (kDown == KEY_A)
		{
			ret=http_download("https://github.com/RyDog199/ACNL-NTR-Cheats/blob/master/ACNL_MULTI.plg?raw=true");
		}
		if (kDown == KEY_B)
		{
			ret=http_download("https://github.com/RyDog199/ACNL-NTR-Cheats/releases/download/v3.0B1/ACNL_MULTI.plg");
		}
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	// Exit services
	httpcExit();
	gfxExit();
	return 0;
}

