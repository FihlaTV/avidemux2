/** *************************************************************************
    \file ADM_asf.cpp
    \brief ASF/WMV demuxer
    copyright            : (C) 2006/2009 by mean
    email                : fixounet@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ADM_default.h"
#include "ADM_Video.h"
#include "ADM_assert.h"

#include "fourcc.h"
#include "DIA_coreToolkit.h"
#include "ADM_asf.h"
#include "ADM_asfPacket.h"

#if 0
#define aprintf printf
#else
#define aprintf(...) {}
#endif

static const uint8_t asf_audio[16]={0x40,0x9e,0x69,0xf8,0x4d,0x5b,0xcf,0x11,0xa8,0xfd,0x00,0x80,0x5f,0x5c,0x44,0x2b};
static const uint8_t asf_video[16]={0xc0,0xef,0x19,0xbc,0x4d,0x5b,0xcf,0x11,0xa8,0xfd,0x00,0x80,0x5f,0x5c,0x44,0x2b};

/** *****************************************
    \fn getHeaders
    \brief Read Headers to collect information 
********************************************/
uint8_t asfHeader::getHeaders(void)
{
  uint32_t i=0,nbSubChunk,hi,lo;
  const chunky *id;
  uint8_t gid[16];
  uint32_t mn=0,mx=0;
  asfChunk chunk(_fd);
  // The first header is header chunk
  chunk.nextChunk();
  id=chunk.chunkId();
  if(id->id!=ADM_CHUNK_HEADER_CHUNK)
  {
    printf("[ASF] expected header chunk\n"); 
    return 0;
  }
  printf("[ASF] getting headers\n");
  chunk.dump();
  nbSubChunk=chunk.read32();
  printf("NB subchunk :%u\n",nbSubChunk);
  chunk.read8();
  chunk.read8();
  for(i=0;i<nbSubChunk;i++)
  {
    asfChunk *s=new asfChunk(_fd);
    uint32_t skip;
    s->nextChunk();
    printf("***************\n");  
    id=s->chunkId();
    s->dump();
    switch(id->id)
    {
   
      case ADM_CHUNK_HEADER_EXTENSION_CHUNK:
      {
        printf("Got header extension chunk\n");
        s->skipChunk();
        break;
        }
#if 0   
        s->skip(16); // Clock type extension ????
        printf("?? %d\n",s->read16());
        printf("?? %d\n",s->read32());
          
        uint32_t streamNameCount;
        uint32_t payloadCount;
          
          asfChunk *u=new asfChunk(_fd);
          for(int zzz=0;zzz<8;zzz++)
          {
              u->nextChunk();
              u->dump();
              id=u->chunkId();
              if(id->id==ADM_CHUNK_EXTENDED_STREAM_PROP)
              {
                  s->skip(8); // start time 
                  s->skip(8); // end time
                  printf("Bitrate         %u :\n",u->read32());
                  printf("Buffer Size     %u :\n",u->read32());
                  printf("BFill           %u :\n",u->read32());
                  printf("Alt Bitrate     %u :\n",u->read32());
                  printf("Alt Bsize       %u :\n",u->read32());
                  printf("Alt Bfullness   %u :\n",u->read32());
                  printf("Max object Size %u :\n",u->read32());
                  printf("Flags           0x%x :\n",u->read32());
                  printf("Stream no       %u :\n",u->read16());
                  printf("Stream lang     %u :\n",u->read16());
                  printf("Stream time/fra %lu :\n",u->read64());
                  streamNameCount=u->read16();
                  payloadCount=u->read16();
                  printf("Stream Nm Count %u :\n",streamNameCount);
                  printf("Payload count   %u :\n",payloadCount);
                  for(int stream=0;stream<streamNameCount;stream++)
                  {
                    u->read16();
                    skip=u->read16();
                    u->skip(skip);
                  }
                  uint32_t size;
                  for(int payload=0;payload<payloadCount;payload++)
                  {
                    for(int pp=0;pp<16;pp++) printf("0x%02x,",u->read8());
                    printf("\n");
                    skip=u->read16();
                    size=u->read32();
                    u->skip(size);
                    printf("Extra Data : %d, skipd %d\n",size,skip);
                  }
                  printf("We are at %x\n",ftello(_fd));
                }
                u->skipChunk();
          }
          delete u;
      }
      break;
#endif      
      case ADM_CHUNK_FILE_HEADER_CHUNK:
        {
            // Client GID
            printf("Client        :");
            for(int z=0;z<16;z++) printf(":%02x",s->read8());
            printf("\n");
            printf("File size     : %08"LLX"\n",s->read64());
            printf("Creation time : %08"LLX"\n",s->read64());
            printf("Number of pack: %08"LLX"\n",s->read64());
            printf("Timestamp 1   : %08"LLX"\n",s->read64());
            _duration=s->read64();
            printf("Timestamp 2   : %08"LLX"\n",_duration);
            printf("Timestamp 3   : %04"LX"\n",s->read32());
            printf("Preload       : %04"LX"\n",s->read32());
            printf("Flags         : %04"LX"\n",s->read32());
            mx=s->read32();
            mn=s->read32();
            if(mx!=mn)
            {
              printf("Variable packet size!!\n");
              delete s;
              return 0; 
            }
            _packetSize=mx;
            printf("Min size      : %04"LX"\n",mx);
            printf("Max size      : %04"LX"\n",mn);
            printf("Uncompres.size: %04"LX"\n",s->read32());
          }
          break;
      case ADM_CHUNK_STREAM_HEADER_CHUNK:
      {
         // Client GID
        uint32_t audiovideo=0; // video=1, audio=2, 0=unknown
        uint32_t sid;
        s->read(gid,16);
        printf("Type            :");
        for(int z=0;z<16;z++) printf("0x%02x,",gid[z]);
        if(!memcmp(gid,asf_video,16))
        {
          printf("(video)");
          audiovideo=1;
        } else
        {
          if(!memcmp(gid,asf_audio,16))
          {
            printf("(audio)"); 
            audiovideo=2;
          } else printf("(? ? ? ?)"); 
        }
        printf("\nConceal       :");
        for(int z=0;z<16;z++) printf(":%02x",s->read8());
        printf("\n");
        printf("Reserved    : %08"LLX"\n",s->read64());
        printf("Total Size  : %04"LX"\n",s->read32());
        printf("Size        : %04"LX"\n",s->read32());
        sid=s->read16();
        printf("Stream nb   : %04d\n",sid);
        printf("Reserved    : %04"LX"\n",s->read32());
        switch(audiovideo)
        {
          case 1: // Video
          {
                    _videoStreamId=sid;
                    if(!loadVideo(s))
                    {
                      delete s;
                      return 0; 
                    }
                    break;
          }
              break;
          case 2: // audio
          {
            loadAudio(s,sid);
          
            
          }
          break;
          default:break; 
          
        }
      }
      break;
       default:
         break;
    }
    s->skipChunk();
    delete s;
  }
  printf("End of headers\n");
  return 1;
}
/**
    \fn loadAudio
*/

bool asfHeader::loadAudio(asfChunk *s,uint32_t sid)
{
  asfAudioTrak *trk=&(_allAudioTracks[_nbAudioTrack]);
    ADM_assert(_nbAudioTrack<ASF_MAX_AUDIO_TRACK);
    trk->streamIndex=sid;
    s->read((uint8_t *)&(trk->wavHeader),sizeof(WAVHeader));
    printf("[Asf] Encoding for audio 0x%x\n",trk->wavHeader.encoding);
#ifdef ADM_BIG_ENDIAN
    Endian_WavHeader(&(trk->wavHeader));
#endif

    trk->extraDataLen=s->read16();
    printf("Extension :%u bytes\n",trk->extraDataLen);
    if(trk->extraDataLen)
    {
      trk->extraData=new uint8_t[trk->extraDataLen];
      s->read(trk->extraData,trk->extraDataLen);
    }
      printf("#block in group   :%d\n",s->read8());
      printf("#byte in group    :%d\n",s->read16());
      printf("Align1            :%d\n",s->read16());
      printf("Align2            :%d\n",s->read16());
      _nbAudioTrack++;
    return true;
}
/**
    \fn loadVideo
*/

uint8_t asfHeader::loadVideo(asfChunk *s)
{
  uint32_t w,h,x;
            w=s->read32();
            h=s->read32();
            s->read8();
            x=s->read16();
            _isvideopresent=1;

            memset(&_mainaviheader,0,sizeof(_mainaviheader));
            _mainaviheader.dwWidth=w;
            _mainaviheader.dwHeight=h;
            _video_bih.biWidth=w;
            _video_bih.biHeight=h;
            printf("Pic Width  %04d\n",w);
            printf("Pic Height %04d\n",h);
            printf(" BMP size  %04d (%04d)\n",x,(int)sizeof(ADM_BITMAPINFOHEADER));
            s->read((uint8_t *)&_video_bih,sizeof(ADM_BITMAPINFOHEADER));

		#ifdef ADM_BIG_ENDIAN
			Endian_BitMapInfo(&_video_bih);
		#endif

            _videostream.dwScale=1000;
            _videostream.dwRate=30000;

            _videostream.fccHandler=_video_bih.biCompression;
            printf("Codec : <%s> (%04x)\n",
                    fourCC::tostring(_video_bih.biCompression),_video_bih.biCompression);
            if(fourCC::check(_video_bih.biCompression,(uint8_t *)"DVR "))
            {
              // It is MS DVR, fail so that the mpeg2 indexer can take it from here
              _videostream.fccHandler=_video_bih.biCompression=fourCC::get((uint8_t *)"MPEG");
              printf("This is MSDVR, not ASF\n");
              return 0; 
            }
            printBih(&_video_bih);
#if 1
            if(_video_bih.biSize>sizeof(ADM_BITMAPINFOHEADER))
            {
                    x=_video_bih.biSize;
#else
            if(x>sizeof(ADM_BITMAPINFOHEADER))
            {
#endif
              _videoExtraLen=x-sizeof(ADM_BITMAPINFOHEADER);
              _videoExtraData=new uint8_t[_videoExtraLen];
              s->read(_videoExtraData,_videoExtraLen);
              ADM_info("We have %d bytes of extra data for video.\n",(int)_videoExtraLen);
            }else
            {
                ADM_info("No extra data for video\n");
            }
            return 1;
}
/**
    \fn      buildIndex
    \brief   Scan the file to build an index
    
    Header Chunk
            Chunk
            Chunk
            Chunk
            
    Data chunk
            Chunk
            Chunk
            
    We skip the 1st one, and just read the header of the 2nd one
    
*/
uint8_t asfHeader::buildIndex(void)
{
  uint32_t fSize;
  const chunky *id;
  uint32_t chunkFound;
  uint32_t r=5;
  uint32_t len;
  
  fseeko(_fd,0,SEEK_END);
  fSize=ftello(_fd);
  fseeko(_fd,0,SEEK_SET);
  
  asfChunk h(_fd);
  printf("[ASF] ********** Building index **********\n");
  printf("[ASF] Searching data\n");
  while(r--)
  {
    h.nextChunk();    // Skip headers
    id=h.chunkId();
    h.dump();
    if(id->id==ADM_CHUNK_DATA_CHUNK) break;
    h.skipChunk();
  }
  if(id->id!=ADM_CHUNK_DATA_CHUNK) return 0;
  // Remove leftover from DATA_chunk
 // Unknown	GUID	16
//       Number of packets	UINT64	8
//       Unknown	UINT8	1
//       Unknown	UINT8	1
//   
  h.read32();
  h.read32();
  h.read32();
  h.read32();
  _nbPackets=(uint32_t) h.read64();
  h.read16();
  
  len=h.chunkLen-16-8-2-24;
  
  printf("[ASF] nbPacket  : %u\n",_nbPackets);
  printf("[ASF] len to go : %u\n",len);
  printf("[ASF] scanning data\n");
  _dataStartOffset=ftello(_fd);
  
  // Here we go
  //DIA_working *working=new DIA_working("indexing asf");
  asfPacket *aPacket=new asfPacket(_fd,_nbPackets,_packetSize,
                                   &readQueue,_dataStartOffset);
  uint32_t packet=1;
#define MAXIMAGE (_nbPackets)
  uint32_t sequence=1;
  uint32_t ceilImage=MAXIMAGE;

  nbImage=0;
  
  len=0;
  asfIndex indexEntry;
  memset(&indexEntry,0,sizeof(indexEntry));
  bool first=true;
  while(packet<_nbPackets)
  {
    while(!readQueue.isEmpty())
    {
      asfBit *bit=NULL;
      
      ADM_assert(readQueue.pop((void**)&bit));
      uint64_t dts=bit->dts;
      if(bit->stream==_videoStreamId)
      {
          aprintf(">found packet of size %d seq %d, while curseq =%d\n",bit->len,bit->sequence,curSeq);
          if(bit->sequence!=sequence || first==true)
          {
            if(first==false)
            {
                indexEntry.frameLen=len;
                _index.push_back(indexEntry);
            }
            first=false;
            aprintf("New sequence\n");
            if( ((sequence+1)&0xff)!=(bit->sequence&0xff))
            {
                printf("!!!!!!!!!!!! non continuous sequence %u %u\n",sequence,bit->sequence); 
    #if 0         
                // Let's insert a couple of null frame
                int32_t delta,start,end;
                
                start=256+bit->sequence-sequence-1;
                start&=0xff;
                printf("!!!!!!!!!!!! Delta %d\n",start);
                
                for(int filler=0;filler<start;filler++)
                {
                  tmpIndex[++nbImage].frameLen=0;
                }
    #endif            
            }
            
            
            indexEntry.frameLen=0;
            indexEntry.segNb=bit->sequence;
            indexEntry.packetNb=bit->packet;
            indexEntry.flags=bit->flags;
            indexEntry.dts=dts;
            indexEntry.pts=ADM_NO_PTS;

            for(int z=0;z<_nbAudioTrack;z++)
            {
              indexEntry.audioSeen[z]=_allAudioTracks[z].length;
              indexEntry.audioDts[z]=_allAudioTracks[z].lastDts;
            }
            readQueue.pushBack(bit);
    
            sequence=bit->sequence;
            len=0;
            continue;
          }
          len+=bit->len;
      } // End of video stream Id
      else  // Audio ?
      {
        int found=0;
        for(int i=0;i<_nbAudioTrack && !found;i++)
        {
          if(bit->stream == _allAudioTracks[i].streamIndex)
          {
            
            _allAudioTracks[i].length+=bit->len;
            _allAudioTracks[i].lastDts=bit->dts;
            
            found=1;
          }
        }
        if(!found) 
        {
          printf("Unmapped stream %u\n",bit->stream); 
        }
      }
     delete[] bit->data;
     delete bit;
    }
    //working->update(packet,_nbPackets);

    packet++;
    aPacket->nextPacket(0xff); // All packets
    aPacket->skipPacket();
  }
  delete aPacket;
  //delete working;
  /* Compact index */
  
  fseeko(_fd,_dataStartOffset,SEEK_SET);
  printf("[ASF] %u images found\n",nbImage);
  printf("[ASF] ******** End of buildindex *******\n");

  nbImage=_index.size();;

  _videostream.dwLength=_mainaviheader.dwTotalFrames=nbImage;
  if(!nbImage) return 0;
  
  // Update fps
  // In fact it is an average fps
  //
  float f=_index[nbImage-1].dts;
   f/=nbImage; // average duration of 1 image in us
    if(f<10) f=10;
   f=1000000.*1000./f;
  uint32_t avgFps=(uint32_t) f;
    printf("[Asf] Average fps=%d\n",avgFps);
  
  _videostream.dwScale=1000;
  _videostream.dwRate=(uint32_t)avgFps;;

  return 1;
  
}
