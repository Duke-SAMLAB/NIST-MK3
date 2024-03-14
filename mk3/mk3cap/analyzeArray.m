#!/usr/local/bin/octave
%----------------------------------------------------------------------
% analyzeArray.m
%----------------------------------------------------------------------
% Purpose: input an array raw file and upconvert it
%
% April 9, 2008
%
% Author: Vince Stanford, NIST Smart Space Project
%----------------------------------------------------------------------
program_name

channels=64;
bytesPerSamp=3;

EXPECTED_ARGC=5;
ARRAY_INPUT_FILE_NAME=1;
FIRST_CHANNEL_NUMBER=2;
SECOND_CHANNEL_NUMBER=3;
FIRST_FRAME=4;
FRAME_COUNT=5;

argc=(size(argv))(1)
if (argc!=EXPECTED_ARGC)
   printf("\nargc=%i whe %i were expected\n\n",argc,EXPECTED_ARGC)
   printf(">>> usage %s inputFileName firstChannelNumber secondChannelNumber firstFrame frameCount\n",program_name)
   exit(0)
end

%------------------------------------------
% get the arg list options
%------------------------------------------
arrayRawFileName=nth(argv,ARRAY_INPUT_FILE_NAME)
xChan=str2num(nth(argv,FIRST_CHANNEL_NUMBER))
yChan=str2num(nth(argv,SECOND_CHANNEL_NUMBER))
firstFrame=str2num(nth(argv,FIRST_FRAME))
frames=str2num(nth(argv,FRAME_COUNT))

%-------------------------
% Read the raw array data
%-------------------------
[arrayFileStat, err, msg] = stat(arrayRawFileName);

arrayBytes=arrayFileStat.size
arrayRawFileID=fopen(arrayRawFileName,"r","native");

fseek(arrayRawFileID,firstFrame*channels*bytesPerSamp);

[arrayRaw,bytesRead]=fread(arrayRawFileID,frames*3*64,'uint8');

if (bytesRead!=(channels*bytesPerSamp*frames))
   printf("error: could only read %i bytes of data\n",bytesRead);
   exit(0)
endif

%------------------------------------------------------
% convert from three byte big endian to octave numeric
%------------------------------------------------------
arraySampBytes=reshape(arrayRaw,bytesPerSamp,frames*channels);
highOrder=arraySampBytes(1,:);
highOrder=(-128-128*sign(highOrder-128.5))+highOrder;
arraySamps=arraySampBytes(3,:)+arraySampBytes(2,:)*256+highOrder*256*256;
arrayData=reshape(arraySamps,channels,frames);

%------------------------------
% Conduct analysis of channels
%------------------------------
figure(1);
chanRMS=std(arrayData');
plot(chanRMS,"@")

figure(2);
x=arrayData(xChan,:);
plot(x)

xMean=mean(x)
xSDev=std(x)

figure(3);
y=arrayData(yChan,:);
plot(y)

yM=mean(y)
ySDev=std(y)

dB=20*log10(xSDev/ySDev)

figure(4);
plot(x,y,".")

correlation=cor(x,y)

%figure(5);
%lag=1
%xIndex=1:(frames-lag);
%yIndex=(lag+1):frames;

%plot(x(xIndex),y(yIndex),".")

%figure(5);
%plot(autocor(y,250),"@")

input("press enter to exit");

exit(0)
