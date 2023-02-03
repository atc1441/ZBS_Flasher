import random

# Original C code by Dmitry: http://dmitry.gr/?r=05.Projects&proj=29.%20eInk%20Price%20Tags
# Ported by ATC1441 (Aaron Christophel) to python on 10.10.2022

in_bytes_pos = 0
out_buffer = []
input_image = []

def save_file(filename):
    global out_buffer
    f = open(filename, 'wb')
    f.write(bytearray(out_buffer))
    f.close()
    
def putchar(the_byte):
    global out_buffer
    the_byte = the_byte & 0xff
    out_buffer = out_buffer + [the_byte]

def putBytes(data, len):
    for n in range(len):
        putchar(data[n])

def getBytes(len):
    global in_bytes_pos
    global input_image
    temp_position = in_bytes_pos
    in_bytes_pos = in_bytes_pos + len
    return input_image[temp_position:in_bytes_pos]
    
def arrToInt(data, len = 4):
    out_int = 0
    for i in range(len):
        out_int |= data[i] << (8*i)
    return out_int
    
def intToArr(data, len = 4):
    out_arr = []
    for i in range(len):
        out_arr = out_arr + [(data >> (8*i))&0xff]
    return out_arr

def repackPackedVals(val, pixelsPerPackedUnit, packedMultiplyVal):
    ret = 0
    for i in range(pixelsPerPackedUnit):
        ret = int(ret * packedMultiplyVal) + int(val % packedMultiplyVal);
        val = int(val / packedMultiplyVal);
    return ret & 0xffffffff

def convertImage(dither, method, inFile, outFile):
    global in_bytes_pos
    global out_buffer
    global input_image
    
    method = method.lower()
    
    in_bytes_pos = 0
    out_buffer = []
    input_image = []
    
    f = open(inFile, 'rb')
    input_image = f.read()
    f.close()
    
    rowBytesOut = 0
    rowBytesIn = 0
    outBpp = 0
    i = 0
    numRows = 0
    pixelsPerPackedUnit = 1
    packedMultiplyVal = 0x01000000
    packedOutBpp = 0
    compressionFormat = 0;
    
    numGrays = 0
    extraColor = 0;
    #struct BitmapFileHeader hdr;
    clut0 = list(bytes(256))
    clut1 = list(bytes(256))
    clut2 = list(bytes(256))
    skipBytes = 0
    
    hdr_sig0 = getBytes(1)
    hdr_sig1 = getBytes(1)
    hdr_fileSz = arrToInt(getBytes(4))
    getBytes(4) # rfu
    hdr_dataOfst = arrToInt(getBytes(4))
    hdr_headerSz = arrToInt(getBytes(4))
    hdr_width = arrToInt(getBytes(4))
    hdr_height = arrToInt(getBytes(4))
    hdr_colorplanes = arrToInt(getBytes(2),2)
    hdr_bpp = arrToInt(getBytes(2),2)
    hdr_compression = arrToInt(getBytes(4))
    hdr_dataLen = arrToInt(getBytes(4))
    hdr_pixelsPerMeterX = arrToInt(getBytes(4))
    hdr_pixelsPerMeterY = arrToInt(getBytes(4))
    hdr_numColors = arrToInt(getBytes(4))
    hdr_numImportantColors = arrToInt(getBytes(4))
    
    if hdr_sig0 != b'B' or hdr_sig1 != b'M' or hdr_headerSz < 40 or hdr_colorplanes != 1 or hdr_bpp != 24 or hdr_compression:
        print("Error input BMP invalid")
        return 1
        
    if method == "1bpp".lower():
        numGrays = 2
        outBpp = 1
    elif method == "1bppY".lower():
        extraColor = 0xffff00
        numGrays = 2
        outBpp = 2
    elif method == "1bppR".lower():
        extraColor = 0xff0000
        numGrays = 2
        outBpp = 2
    elif method == "3clrPkdY".lower():
        numGrays = 2
        extraColor = 0xffff00
        outBpp = 2
        packedOutBpp = 8
        pixelsPerPackedUnit = 5
        packedMultiplyVal = 3
        compressionFormat = 0x62700538 # 5 pixels of 3 possible colors in 8 bits
    elif method == "3clrPkdR".lower():
        numGrays = 2
        extraColor = 0xff0000
        outBpp = 2
        packedOutBpp = 8
        pixelsPerPackedUnit = 5
        packedMultiplyVal = 3
        compressionFormat = 0x62700538 # 5 pixels of 3 possible colors in 8 bits
    elif method == "2bpp".lower():
        numGrays = 4
        outBpp = 2
    elif method == "2bppY".lower():
        numGrays = 3
        extraColor = 0xffff00
        outBpp = 2
    elif method == "2bppR".lower():
        numGrays = 3
        extraColor = 0xff0000
        outBpp = 2
    elif method == "5clrPkdY".lower():
        numGrays = 4
        extraColor = 0xffff00
        outBpp = 3
        packedOutBpp = 7
        pixelsPerPackedUnit = 3
        packedMultiplyVal = 5
        compressionFormat = 0x62700357 # 3 pixels of 5 possible colors in 7 bits
    elif method == "5clrPkdR".lower():
        numGrays = 4
        extraColor = 0xff0000
        outBpp = 3
        packedOutBpp = 7
        pixelsPerPackedUnit = 3
        packedMultiplyVal = 5
        compressionFormat = 0x62700357 # 3 pixels of 5 possible colors in 7 bits
    elif method == "6clrPkdY".lower():
        numGrays = 5
        extraColor = 0xffff00
        outBpp = 3
        packedOutBpp = 8
        pixelsPerPackedUnit = 3
        packedMultiplyVal = 6
        compressionFormat = 0x62700368 # 3 pixels of 6 possible colors in 8 bits
    elif method == "6clrPkdR".lower():
        numGrays = 5
        extraColor = 0xff0000
        outBpp = 3
        packedOutBpp = 8
        pixelsPerPackedUnit = 3
        packedMultiplyVal = 6
        compressionFormat = 0x62700368 # 3 pixels of 6 possible colors in 8 bits
    elif method == "3bpp".lower():
        numGrays = 8
        outBpp = 3
    elif method == "3bppY".lower():
        numGrays = 7
        extraColor = 0xffff00
        outBpp = 3
    elif method == "3bppR".lower():
        numGrays = 7
        extraColor = 0xff0000
        outBpp = 3
    elif method == "4bpp".lower():
        numGrays = 16
        outBpp = 4
    else:
        prin("Error no method supplied")
        return 1
        
    if packedOutBpp == 0:
        packedOutBpp = outBpp
        
    skipBytes = hdr_dataOfst - 54
    if skipBytes < 0:
        print("HDR too short")
        return 1
    getBytes(skipBytes)
    
    rowBytesIn = int(int((hdr_width * hdr_bpp) / 32) * 4)
    
    #first sort out how many pixel packages we'll have and round up
    rowBytesOut = int(((hdr_width + pixelsPerPackedUnit - 1) / pixelsPerPackedUnit) * packedOutBpp)
    #the convert that to row bytes (round up to nearest multiple of 4 bytes)
    rowBytesOut = int(int((rowBytesOut + 31) / 32) * 4)
    numRows = hdr_height;
    
    tempNumColors = 1 << outBpp
    tempDataOfst = int(54 + 4 * tempNumColors)
    tempDataLen = int(numRows * rowBytesOut)
    putchar(0x42)
    putchar(0x4D)
    putBytes(intToArr(tempDataOfst + tempDataLen),4) # File size
    putBytes([0,0,0,0],4) # rfu
    putBytes(intToArr(tempDataOfst),4) # Data Ofset
    putBytes(intToArr(40),4) # Header size
    putBytes(intToArr(hdr_width),4) # Width
    putBytes(intToArr(hdr_height),4) # Height
    putBytes(intToArr(hdr_colorplanes,2),2) # Colorplanes
    putBytes(intToArr(outBpp,2),2) # bpp
    putBytes(intToArr(compressionFormat),4) # Compression
    putBytes(intToArr(tempDataLen),4) # Data length
    putBytes(intToArr(hdr_pixelsPerMeterX),4) # PixelsPerMeterX
    putBytes(intToArr(hdr_pixelsPerMeterY),4) # PixelsPerMeterY
    putBytes(intToArr(tempNumColors),4) # NumColors
    putBytes(intToArr(tempNumColors),4) # NumImportantColors
    
	#emit & record grey clut entries
    for i in range(numGrays):
        val = int(255*i / (numGrays - 1))
        putchar(val)    
        putchar(val)    
        putchar(val)    
        putchar(val)    
        clut0[i]=val
        clut1[i]=val
        clut2[i]=val
    
    tempPadding = tempNumColors - numGrays
    #if there is a color CLUT entry, emit that
    if extraColor > 0:
        putchar((extraColor >> 0) & 0xff) # B
        putchar((extraColor >> 8) & 0xff) # G
        putchar((extraColor >> 16) & 0xff) # R
        putchar(0x00)					 # A
        
        clut0[numGrays] = (extraColor >> 0) & 0xff;
        clut1[numGrays] = (extraColor >> 8) & 0xff;
        clut2[numGrays] = (extraColor >> 16) & 0xff;
        tempPadding -= 1     
    
    #pad clut to size    
    for i in range(tempPadding):
        putchar(0x00)
        putchar(0x00)
        putchar(0x00)
        putchar(0x00)
    
    while(numRows):
        numRows-=1
        pixelValsPackedSoFar = 0
        numPixelsPackedSoFar = 0
        valSoFar = 0
        bytesIn = 0
        bytesOut = 0
        bitsSoFar = 0
        
        for c in range(hdr_width):
            bytesIn += 3
            bestDist = 0x7fffffffffffffff
            rgb = getBytes(3)
            bestIdx = 0
            ditherFudge = 0
            
            if dither == 1:
                ditherFudge = int(int(random.randint(0,255) - 127) /int(numGrays))
            
            for col in range(tempNumColors):
                dist = 0
                dist += (rgb[0] - clut0[col] + ditherFudge) * (rgb[0] - clut0[col] + ditherFudge) * 4750
                dist += (rgb[1] - clut1[col] + ditherFudge) * (rgb[1] - clut1[col] + ditherFudge) * 47055
                dist += (rgb[2] - clut2[col] + ditherFudge) * (rgb[2] - clut2[col] + ditherFudge) * 13988                
            
                if dist < bestDist:
                    bestDist = dist
                    bestIdx = col
            
            # pack pixels as needed
            pixelValsPackedSoFar = int(pixelValsPackedSoFar * packedMultiplyVal) + bestIdx
            numPixelsPackedSoFar += 1
            if numPixelsPackedSoFar != pixelsPerPackedUnit:
                continue
            
            numPixelsPackedSoFar = 0
            
            #it is easier to display when low val is firts pixel. currently last pixel is low - reverse this
            pixelValsPackedSoFar = repackPackedVals(pixelValsPackedSoFar, pixelsPerPackedUnit, packedMultiplyVal)
            
            valSoFar = (valSoFar << packedOutBpp) | pixelValsPackedSoFar
            pixelValsPackedSoFar = 0
            bitsSoFar += packedOutBpp
            
            if bitsSoFar >= 8:
                bitsSoFar -= 8
                putchar(valSoFar >> (bitsSoFar))
                valSoFar &= (1 << bitsSoFar) - 1
                bytesOut += 1
        
		#see if we have unfinished pixel packages to write
        if numPixelsPackedSoFar > 0:
            while(numPixelsPackedSoFar != pixelsPerPackedUnit):
                numPixelsPackedSoFar += 1
                pixelValsPackedSoFar *= packedMultiplyVal;
            
            #it is easier to display when low val is firts pixel. currently last pixel is low - reverse this
            pixelValsPackedSoFar = repackPackedVals(pixelValsPackedSoFar, pixelsPerPackedUnit, packedMultiplyVal)
            
            valSoFar = (valSoFar << packedOutBpp) | pixelValsPackedSoFar
            pixelValsPackedSoFar = 0
            bitsSoFar += packedOutBpp
        
            if bitsSoFar >= 8:
                bitsSoFar -= 8
                putchar(valSoFar >> (bitsSoFar))
                valSoFar &= (1 << bitsSoFar) - 1
                bytesOut += 1
                
        if bitsSoFar > 0:
            valSoFar <<= 8 - bitsSoFar # left-align it as is expected
            putchar(valSoFar)
            bytesOut += 1
                    
        getBytes(rowBytesIn-bytesIn)
        putBytes(list(bytes(rowBytesOut-bytesOut)),rowBytesOut-bytesOut)
    
    save_file(outFile)
    print("done")
    return 0

#convertImage(0, "1bppR", "a.bmp", "out.bin")