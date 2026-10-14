#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include <locale.h>

#if !defined(_WIN32) && (defined(__WIN32__) || defined(WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__))
#define _WIN32 1
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#define EPSILON_TOLERANCE 1e-6

struct MemoryBuffer
{
    char *memory;
    size_t size;
};

typedef struct
{
    double x;
    double y;
} Point;

typedef struct
{
    int isVertical;
    double verticalX;
    double slope;
    double intercept;
    int inlierCount;
    int *inlierIndices;
    char label[64];
    Point startPoint;
    Point endPoint;
} Line;

typedef struct
{
    Point coordinate;
    double distanceToRobot;
    double angleDegrees;
    int firstLineIndex;
    int secondLineIndex;
} Intersection;

double calculateAngleBetweenLinesDegrees(const Line *firstLine, const Line *secondLine);

typedef struct
{
    char timestamp[64];
    char frameId[64];
    double angleMin;
    double angleMax;
    double angleIncrement;
    double rangeMin;
    double rangeMax;
    int rangeCount;
    double *rangeMeasurements;
} LidarScan;

/* Metin basindaki ve sonundaki bosluk, tirnak ve backtick karakterlerini temizler. */
void trimWhitespaceAndQuotes(char *rawText)
{
    char *cursorStart = rawText;
    while (*cursorStart && (isspace((unsigned char)*cursorStart) || *cursorStart == '\"' || *cursorStart == '\'' || *cursorStart == '`'))
    {
        cursorStart++;
    }
    if (cursorStart != rawText)
    {
        memmove(rawText, cursorStart, strlen(cursorStart) + 1);
    }
    char *cursorEnd = rawText + strlen(rawText) - 1;
    while (cursorEnd >= rawText && (isspace((unsigned char)*cursorEnd) || *cursorEnd == '\"' || *cursorEnd == '\'' || *cursorEnd == '`'))
    {
        *cursorEnd-- = '\0';
    }
}

/* Windows ortaminda Turkce/Unicode karakterli dosya yollarini guvenle acabilmek icin _wfopen dener. */
static FILE *tryOpenFilePath(const char *targetFilePath, const char *accessMode)
{
    FILE *fileHandle = fopen(targetFilePath, accessMode);
    if (fileHandle) return fileHandle;

#ifdef _WIN32
    wchar_t widePathBuffer[2048];
    wchar_t wideModeBuffer[16];
    MultiByteToWideChar(CP_UTF8, 0, accessMode, -1, wideModeBuffer, 16);

    // Konsol, sistem ve UTF-8 kod sayfalarini sirayla deneyerek non-ASCII yollari cozer
    if (MultiByteToWideChar(GetConsoleCP(), 0, targetFilePath, -1, widePathBuffer, 2048) > 0)
    {
        fileHandle = _wfopen(widePathBuffer, wideModeBuffer);
        if (fileHandle) return fileHandle;
    }
    if (MultiByteToWideChar(CP_ACP, 0, targetFilePath, -1, widePathBuffer, 2048) > 0)
    {
        fileHandle = _wfopen(widePathBuffer, wideModeBuffer);
        if (fileHandle) return fileHandle;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, targetFilePath, -1, widePathBuffer, 2048) > 0)
    {
        fileHandle = _wfopen(widePathBuffer, wideModeBuffer);
        if (fileHandle) return fileHandle;
    }
#endif
    return NULL;
}

/* Dosyayi dogrudan ya da gerekirse sonuna .toml ekleyerek acmayi dener. */
static FILE *tryOpenWithTomlExtension(const char *targetFilePath, const char *accessMode, char *resolvedPathBuffer, size_t bufferSize)
{
    FILE *fileHandle = tryOpenFilePath(targetFilePath, accessMode);
    if (fileHandle)
    {
        if (resolvedPathBuffer) snprintf(resolvedPathBuffer, bufferSize, "%s", targetFilePath);
        return fileHandle;
    }

    size_t pathLength = strlen(targetFilePath);
    if (pathLength < 5 || (strcmp(targetFilePath + pathLength - 5, ".toml") != 0 && strcmp(targetFilePath + pathLength - 5, ".TOML") != 0))
    {
        char pathWithExtensionBuffer[2048];
        snprintf(pathWithExtensionBuffer, sizeof(pathWithExtensionBuffer), "%s.toml", targetFilePath);
        fileHandle = tryOpenFilePath(pathWithExtensionBuffer, accessMode);
        if (fileHandle)
        {
            if (resolvedPathBuffer) snprintf(resolvedPathBuffer, bufferSize, "%s", pathWithExtensionBuffer);
            return fileHandle;
        }
    }
    return NULL;
}

/* Calisma dizini, Masaustu ve Belgeler gibi yaygin klasorlerde dosyayi arayip acar. */
FILE *smartOpenFile(const char *requestedPath, const char *accessMode)
{
    char resolvedPath[2048];

    FILE *fileHandle = tryOpenWithTomlExtension(requestedPath, accessMode, resolvedPath, sizeof(resolvedPath));
    if (fileHandle) return fileHandle;

#ifdef _WIN32
    const char *userProfileDir = getenv("USERPROFILE");
    if (userProfileDir)
    {
        const char *searchDirectories[] = {
            "Downloads",
            "Desktop",
            "Documents",
            "OneDrive\\Downloads",
            "OneDrive\\Desktop",
            "OneDrive\\Documents",
            "OneDrive\\Masaüstü",
            "OneDrive\\Belgeler",
            "Masaüstü",
            "Belgeler",
            "İndirilenler"
        };
        int searchDirectoryCount = sizeof(searchDirectories) / sizeof(searchDirectories[0]);

        for (int i = 0; i < searchDirectoryCount; i++)
        {
            char candidatePath[2048];
            snprintf(candidatePath, sizeof(candidatePath), "%s\\%s\\%s", userProfileDir, searchDirectories[i], requestedPath);
            fileHandle = tryOpenWithTomlExtension(candidatePath, accessMode, resolvedPath, sizeof(resolvedPath));
            if (fileHandle)
            {
                printf("Dosya bulundu:\n-> %s\n", resolvedPath);
                return fileHandle;
            }
        }
    }

    char relativeCandidatePath[2048];
    snprintf(relativeCandidatePath, sizeof(relativeCandidatePath), "..\\%s", requestedPath);
    fileHandle = tryOpenWithTomlExtension(relativeCandidatePath, accessMode, resolvedPath, sizeof(resolvedPath));
    if (fileHandle)
    {
        printf("Dosya bulundu:\n-> %s\n", resolvedPath);
        return fileHandle;
    }

    snprintf(relativeCandidatePath, sizeof(relativeCandidatePath), "bin\\Debug\\%s", requestedPath);
    fileHandle = tryOpenWithTomlExtension(relativeCandidatePath, accessMode, resolvedPath, sizeof(resolvedPath));
    if (fileHandle)
    {
        printf("Dosya bulundu:\n-> %s\n", resolvedPath);
        return fileHandle;
    }
#endif

    return NULL;
}

/* libcurl veri indirme geri cagirim fonksiyonu (dinamik bellek yonetimi) */
static size_t writeMemoryCallback(void *downloadedContents, size_t elementSize, size_t elementCount, void *userMemoryBuffer)
{
    size_t totalBytes = elementSize * elementCount;
    struct MemoryBuffer *memoryBuffer = (struct MemoryBuffer *)userMemoryBuffer;
    char *reallocatedMemory = realloc(memoryBuffer->memory, memoryBuffer->size + totalBytes + 1);
    if (reallocatedMemory == NULL)
    {
        fprintf(stderr, "bellek yetersiz (realloc basarisiz)\n");
        return 0;
    }
    memoryBuffer->memory = reallocatedMemory;
    memcpy(&(memoryBuffer->memory[memoryBuffer->size]), downloadedContents, totalBytes);
    memoryBuffer->size += totalBytes;
    memoryBuffer->memory[memoryBuffer->size] = '\0';
    return totalBytes;
}

/* Verilen URL adresindeki TOML dosyasini yerel diske indirir. */
int downloadRemoteFile(const char *sourceUrl, const char *destinationFilePath)
{
    CURL *curlHandle = curl_easy_init();
    if (!curlHandle)
    {
        fprintf(stderr, "curl_easy_init() basarisiz\n");
        return 0;
    }

    struct MemoryBuffer downloadChunk;
    downloadChunk.memory = malloc(1);
    downloadChunk.size = 0;

    curl_easy_setopt(curlHandle, CURLOPT_URL, sourceUrl);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeMemoryCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, (void *)&downloadChunk);
    curl_easy_setopt(curlHandle, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode curlResult = curl_easy_perform(curlHandle);
    curl_easy_cleanup(curlHandle);

    if (curlResult != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() basarisiz: %s\n", curl_easy_strerror(curlResult));
        free(downloadChunk.memory);
        return 0;
    }

    FILE *outputFileHandle = fopen(destinationFilePath, "wb");
    if (!outputFileHandle)
    {
        perror("Download: Cikti dosyasi acilamadi");
        free(downloadChunk.memory);
        return 0;
    }

    fwrite(downloadChunk.memory, 1, downloadChunk.size, outputFileHandle);
    fclose(outputFileHandle);
    free(downloadChunk.memory);
    return 1;
}

/* TOML formatindaki LIDAR tarama verilerini ve baslik bilgilerini ayiklar. */
LidarScan parseTomlScanFile(const char *scanFilePath)
{
    FILE *fileHandle = smartOpenFile(scanFilePath, "r");
    if (!fileHandle)
    {
        fprintf(stderr, "\nHATA: '%s' dosyasi bulunamadi veya acilamadi!\n", scanFilePath);
        fprintf(stderr, "Dosya adini (.toml uzantisi dahil) kontrol edin veya tam yolunu girin.\n");
        exit(1);
    }

    LidarScan scanData = {0};
    char lineBuffer[65536];
    int isParsingRanges = 0;
    int rangesCapacity = 256;
    scanData.rangeMeasurements = malloc(sizeof(double) * rangesCapacity);
    if (!scanData.rangeMeasurements)
    {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    while (fgets(lineBuffer, sizeof(lineBuffer), fileHandle))
    {
        trimWhitespaceAndQuotes(lineBuffer);
        if (strlen(lineBuffer) == 0) continue;

        if (strncmp(lineBuffer, "stamp", 5) == 0)
        {
            char *equalsSignPointer = strchr(lineBuffer, '=');
            if (equalsSignPointer)
            {
                equalsSignPointer++;
                while (*equalsSignPointer == ' ') equalsSignPointer++;
                char *valueStart = equalsSignPointer;
                char *valueEnd;
                if (*valueStart == '"')
                {
                    valueStart++;
                    valueEnd = strchr(valueStart, '"');
                    if (valueEnd) *valueEnd = '\0';
                }
                else
                {
                    valueEnd = valueStart + strlen(valueStart);
                }
                strncpy(scanData.timestamp, valueStart, sizeof(scanData.timestamp) - 1);
                scanData.timestamp[sizeof(scanData.timestamp) - 1] = '\0';
            }
        }
        else if (strncmp(lineBuffer, "frame_id", 8) == 0)
        {
            char *equalsSignPointer = strchr(lineBuffer, '=');
            if (equalsSignPointer)
            {
                equalsSignPointer++;
                while (*equalsSignPointer == ' ') equalsSignPointer++;
                char *valueStart = equalsSignPointer;
                char *valueEnd;
                if (*valueStart == '"')
                {
                    valueStart++;
                    valueEnd = strchr(valueStart, '"');
                    if (valueEnd) *valueEnd = '\0';
                }
                else
                {
                    valueEnd = valueStart + strlen(valueStart);
                }
                strncpy(scanData.frameId, valueStart, sizeof(scanData.frameId) - 1);
                scanData.frameId[sizeof(scanData.frameId) - 1] = '\0';
            }
        }
        else if (strncmp(lineBuffer, "angle_min", 9) == 0)
        {
            sscanf(lineBuffer, "angle_min = %lf", &scanData.angleMin);
        }
        else if (strncmp(lineBuffer, "angle_max", 9) == 0)
        {
            sscanf(lineBuffer, "angle_max = %lf", &scanData.angleMax);
        }
        else if (strncmp(lineBuffer, "angle_increment", 15) == 0)
        {
            sscanf(lineBuffer, "angle_increment = %lf", &scanData.angleIncrement);
        }
        else if (strncmp(lineBuffer, "range_min", 9) == 0)
        {
            sscanf(lineBuffer, "range_min = %lf", &scanData.rangeMin);
        }
        else if (strncmp(lineBuffer, "range_max", 9) == 0)
        {
            sscanf(lineBuffer, "range_max = %lf", &scanData.rangeMax);
        }
        else if (strstr(lineBuffer, "ranges = ["))
        {
            isParsingRanges = 1;
            char *bracketPointer = strchr(lineBuffer, '[');
            if (bracketPointer) bracketPointer++;
            while (*bracketPointer)
            {
                double parsedRangeValue;
                if (sscanf(bracketPointer, "%lf", &parsedRangeValue) == 1)
                {
                    if (scanData.rangeCount >= rangesCapacity)
                    {
                        rangesCapacity *= 2;
                        double *reallocatedRanges = realloc(scanData.rangeMeasurements, sizeof(double) * rangesCapacity);
                        if (!reallocatedRanges)
                        {
                            free(scanData.rangeMeasurements);
                            fprintf(stderr, "realloc failed\n");
                            exit(1);
                        }
                        scanData.rangeMeasurements = reallocatedRanges;
                    }
                    scanData.rangeMeasurements[scanData.rangeCount++] = parsedRangeValue;
                }
                while (*bracketPointer && *bracketPointer != ',' && *bracketPointer != ']') bracketPointer++;
                if (*bracketPointer == ']')
                {
                    isParsingRanges = 0;
                    break;
                }
                if (*bracketPointer) bracketPointer++;
            }
        }
        else if (isParsingRanges)
        {
            char *bracketPointer = lineBuffer;
            while (*bracketPointer)
            {
                double parsedRangeValue;
                if (sscanf(bracketPointer, "%lf", &parsedRangeValue) == 1)
                {
                    if (scanData.rangeCount >= rangesCapacity)
                    {
                        rangesCapacity *= 2;
                        double *reallocatedRanges = realloc(scanData.rangeMeasurements, sizeof(double) * rangesCapacity);
                        if (!reallocatedRanges)
                        {
                            free(scanData.rangeMeasurements);
                            fprintf(stderr, "realloc failed\n");
                            exit(1);
                        }
                        scanData.rangeMeasurements = reallocatedRanges;
                    }
                    scanData.rangeMeasurements[scanData.rangeCount++] = parsedRangeValue;
                }
                while (*bracketPointer && *bracketPointer != ',' && *bracketPointer != ']') bracketPointer++;
                if (*bracketPointer == ']')
                {
                    isParsingRanges = 0;
                    break;
                }
                if (*bracketPointer) bracketPointer++;
            }
        }
    }
    fclose(fileHandle);
    return scanData;
}

/* Kutupsal LIDAR olcumlerini Kartezyen (X, Y) koordinat sistemine donusturur. */
Point *convertPolarToCartesian(const LidarScan *scanData, int *cartesianPointCount)
{
    int totalReadings = scanData->rangeCount;
    Point *cartesianPoints = malloc(sizeof(Point) * totalReadings);
    int validPointCount = 0;

    for (int i = 0; i < totalReadings; i++)
    {
        double measuredDistance = scanData->rangeMeasurements[i];
        if (measuredDistance < 0) continue;

        double beamAngle = scanData->angleMin + i * scanData->angleIncrement;
        if (beamAngle > scanData->angleMax + EPSILON_TOLERANCE) break;

        cartesianPoints[validPointCount].x = measuredDistance * cos(beamAngle);
        cartesianPoints[validPointCount].y = measuredDistance * sin(beamAngle);
        validPointCount++;
    }
    *cartesianPointCount = validPointCount;
    return cartesianPoints;
}

/* Min/max aralik disi, NaN veya hatali sensor okumalarini eler (-1 olarak isaretler). */
void filterInvalidLaserRanges(LidarScan *scanData)
{
    for (int i = 0; i < scanData->rangeCount; i++)
    {
        double measuredDistance = scanData->rangeMeasurements[i];
        if (isnan(measuredDistance) || isinf(measuredDistance) || measuredDistance < scanData->rangeMin || measuredDistance > scanData->rangeMax)
        {
            scanData->rangeMeasurements[i] = -1.0;
        }
    }
}

/* Bir noktanin dogruya olan dik (Oklid) uzakligini hesaplar. */
double calculatePointToLineDistance(Point queryPoint, const Line *targetLine)
{
    if (targetLine->isVertical)
    {
        return fabs(queryPoint.x - targetLine->verticalX);
    }
    return fabs(targetLine->slope * queryPoint.x - queryPoint.y + targetLine->intercept) / sqrt(targetLine->slope * targetLine->slope + 1.0);
}

/* Iki noktadan gecen dogru denklemini (y = mx + b veya dusey dogru) olusturur. */
Line createLineFromTwoPoints(Point firstPoint, Point secondPoint)
{
    Line constructedLine = {0};
    constructedLine.startPoint = firstPoint;
    constructedLine.endPoint   = secondPoint;

    if (fabs(secondPoint.x - firstPoint.x) < 1e-8)
    {
        constructedLine.isVertical = 1;
        constructedLine.verticalX  = firstPoint.x;
    }
    else
    {
        constructedLine.isVertical = 0;
        constructedLine.slope      = (secondPoint.y - firstPoint.y) / (secondPoint.x - firstPoint.x);
        constructedLine.intercept  = firstPoint.y - constructedLine.slope * firstPoint.x;
    }
    return constructedLine;
}


/* Nokta kumesine Temel Bilesen Analizi (PCA) ile dogru uydurur (dusey dogrulara dayanikli). */
int fitLineUsingPCA(const Point *pointArray, const int *inlierIndices, int inlierCount, Line *fittedLineOutput)
{
    if (inlierCount < 2) return 0;

    double meanX = 0.0, meanY = 0.0;
    for (int i = 0; i < inlierCount; i++)
    {
        meanX += pointArray[inlierIndices[i]].x;
        meanY += pointArray[inlierIndices[i]].y;
    }
    meanX /= inlierCount;
    meanY /= inlierCount;

    double covarianceXX = 0.0, covarianceYY = 0.0, covarianceXY = 0.0;
    for (int i = 0; i < inlierCount; i++)
    {
        double deltaX = pointArray[inlierIndices[i]].x - meanX;
        double deltaY = pointArray[inlierIndices[i]].y - meanY;
        covarianceXX += deltaX * deltaX;
        covarianceYY += deltaY * deltaY;
        covarianceXY += deltaX * deltaY;
    }

    // 2x2 kovaryans matrisinin ozvektor acisiyla dogrunun normal vektoru hesaplanir
    double orientationAngle = 0.5 * atan2(2.0 * covarianceXY, covarianceXX - covarianceYY);
    double normalVectorX = -sin(orientationAngle);
    double normalVectorY =  cos(orientationAngle);

    if (fabs(normalVectorY) < 1e-4)
    {
        fittedLineOutput->isVertical = 1;
        fittedLineOutput->verticalX  = meanX;
    }
    else
    {
        fittedLineOutput->isVertical = 0;
        fittedLineOutput->slope      = -normalVectorX / normalVectorY;
        fittedLineOutput->intercept  = meanY - fittedLineOutput->slope * meanX;
    }
    return 1;
}


/* RANSAC ve PCA kullanarak nokta bulutundan dogrusal duvar segmentlerini cikarir. */
Line *extractWallsUsingRANSAC(Point *laserPoints, int totalLaserPoints, int *extractedLineCount)
{
    const int    MAX_RANSAC_ITERATIONS = 3000;
    const int    MIN_INLIER_THRESHOLD  = 8;
    const int    MAX_DETECTABLE_LINES  = 50;
    const double DISTANCE_TOLERANCE    = 0.025;
    const double MIN_SEED_DIST_SQ      = 0.03 * 0.03;
    const double MIN_WALL_LENGTH       = 0.22;

    int *pointOwnerLineIndex = (int *)malloc(sizeof(int) * totalLaserPoints);
    if (!pointOwnerLineIndex) { fprintf(stderr, "malloc failed\n"); exit(1); }
    for (int i = 0; i < totalLaserPoints; i++) pointOwnerLineIndex[i] = -1;

    int *bestInlierIndices      = (int *)malloc(sizeof(int) * totalLaserPoints);
    int *candidateInlierIndices = (int *)malloc(sizeof(int) * totalLaserPoints);
    if (!bestInlierIndices || !candidateInlierIndices) { fprintf(stderr, "malloc failed\n"); exit(1); }

    Line *detectedLines = (Line *)malloc(sizeof(Line) * MAX_DETECTABLE_LINES);
    if (!detectedLines) { fprintf(stderr, "malloc failed\n"); exit(1); }

    int detectedLineCount = 0;
    int consecutiveFailedAttempts = 0;
    srand(5);

    while (detectedLineCount < MAX_DETECTABLE_LINES && consecutiveFailedAttempts < 15)
    {
        int bestCandidateInlierCount = 0;
        Line bestCandidateLine = (Line){0};

        for (int iteration = 0; iteration < MAX_RANSAC_ITERATIONS; iteration++)
        {
            int firstSeedIndex  = rand() % totalLaserPoints;
            int secondSeedIndex = rand() % totalLaserPoints;

            if (pointOwnerLineIndex[firstSeedIndex] != -1 || pointOwnerLineIndex[secondSeedIndex] != -1 || firstSeedIndex == secondSeedIndex)
            {
                continue;
            }

            double seedDeltaX = laserPoints[firstSeedIndex].x - laserPoints[secondSeedIndex].x;
            double seedDeltaY = laserPoints[firstSeedIndex].y - laserPoints[secondSeedIndex].y;
            double seedDistanceSquared = seedDeltaX * seedDeltaX + seedDeltaY * seedDeltaY;
            if (seedDistanceSquared < MIN_SEED_DIST_SQ) continue;

            Line trialLine = createLineFromTwoPoints(laserPoints[firstSeedIndex], laserPoints[secondSeedIndex]);

            int trialInlierCount = 0;
            for (int pointIndex = 0; pointIndex < totalLaserPoints; pointIndex++)
            {
                if (pointOwnerLineIndex[pointIndex] >= 0 && pointOwnerLineIndex[pointIndex] < detectedLineCount)
                {
                    // Kose noktalarinin ortak kullanimina yalnizca belirgin acilarda (>= 45 derece) izin ver
                    if (calculateAngleBetweenLinesDegrees(&trialLine, &detectedLines[pointOwnerLineIndex[pointIndex]]) < 45.0)
                        continue;
                }
                else if (pointOwnerLineIndex[pointIndex] != -1)
                {
                    continue;
                }

                if (calculatePointToLineDistance(laserPoints[pointIndex], &trialLine) <= DISTANCE_TOLERANCE)
                {
                    candidateInlierIndices[trialInlierCount++] = pointIndex;
                }
            }

            if (trialInlierCount > bestCandidateInlierCount)
            {
                bestCandidateInlierCount = trialInlierCount;
                memcpy(bestInlierIndices, candidateInlierIndices, sizeof(int) * bestCandidateInlierCount);
                fitLineUsingPCA(laserPoints, bestInlierIndices, bestCandidateInlierCount, &bestCandidateLine);
            }
        }

        if (bestCandidateInlierCount < MIN_INLIER_THRESHOLD)
        {
            consecutiveFailedAttempts++;
            continue;
        }

        int refinedInlierCount = 0;
        for (int pointIndex = 0; pointIndex < totalLaserPoints; pointIndex++)
        {
            if (pointOwnerLineIndex[pointIndex] >= 0 && pointOwnerLineIndex[pointIndex] < detectedLineCount)
            {
                if (calculateAngleBetweenLinesDegrees(&bestCandidateLine, &detectedLines[pointOwnerLineIndex[pointIndex]]) < 45.0)
                    continue;
            }
            else if (pointOwnerLineIndex[pointIndex] != -1)
            {
                continue;
            }

            if (calculatePointToLineDistance(laserPoints[pointIndex], &bestCandidateLine) <= DISTANCE_TOLERANCE)
            {
                candidateInlierIndices[refinedInlierCount++] = pointIndex;
            }
        }

        if (refinedInlierCount >= MIN_INLIER_THRESHOLD)
        {
            bestCandidateInlierCount = refinedInlierCount;
            memcpy(bestInlierIndices, candidateInlierIndices, sizeof(int) * bestCandidateInlierCount);
            fitLineUsingPCA(laserPoints, bestInlierIndices, bestCandidateInlierCount, &bestCandidateLine);
        }

        int projectAlongYAxis = bestCandidateLine.isVertical || (fabs(bestCandidateLine.slope) > 1.0);
        for (int a = 0; a < bestCandidateInlierCount - 1; a++)
        {
            for (int b = a + 1; b < bestCandidateInlierCount; b++)
            {
                double coordinateA = projectAlongYAxis ? laserPoints[bestInlierIndices[a]].y : laserPoints[bestInlierIndices[a]].x;
                double coordinateB = projectAlongYAxis ? laserPoints[bestInlierIndices[b]].y : laserPoints[bestInlierIndices[b]].x;
                if (coordinateA > coordinateB)
                {
                    int temporarySwapIndex = bestInlierIndices[a];
                    bestInlierIndices[a] = bestInlierIndices[b];
                    bestInlierIndices[b] = temporarySwapIndex;
                }
            }
        }

        // Kapi veya bosluklarla ayrilmis ayni dogrultudaki duvarlari ayir (> 35 cm)
        const double MAX_NEIGHBOR_GAP_SQ = 0.35 * 0.35;
        int longestClusterStartIndex = 0;
        int longestClusterLength = 1;
        int currentClusterStartIndex = 0;
        int currentClusterLength = 1;

        for (int j = 0; j < bestCandidateInlierCount - 1; j++)
        {
            double deltaX = laserPoints[bestInlierIndices[j + 1]].x - laserPoints[bestInlierIndices[j]].x;
            double deltaY = laserPoints[bestInlierIndices[j + 1]].y - laserPoints[bestInlierIndices[j]].y;
            if (deltaX * deltaX + deltaY * deltaY <= MAX_NEIGHBOR_GAP_SQ)
            {
                currentClusterLength++;
            }
            else
            {
                if (currentClusterLength > longestClusterLength)
                {
                    longestClusterLength = currentClusterLength;
                    longestClusterStartIndex = currentClusterStartIndex;
                }
                currentClusterStartIndex = j + 1;
                currentClusterLength = 1;
            }
        }
        if (currentClusterLength > longestClusterLength)
        {
            longestClusterLength = currentClusterLength;
            longestClusterStartIndex = currentClusterStartIndex;
        }

        if (longestClusterLength < MIN_INLIER_THRESHOLD)
        {
            consecutiveFailedAttempts++;
            continue;
        }

        for (int k = 0; k < longestClusterLength; k++)
        {
            bestInlierIndices[k] = bestInlierIndices[longestClusterStartIndex + k];
        }
        bestCandidateInlierCount = longestClusterLength;
        fitLineUsingPCA(laserPoints, bestInlierIndices, bestCandidateInlierCount, &bestCandidateLine);

        double minSpanCoordinate = projectAlongYAxis ? laserPoints[bestInlierIndices[0]].y : laserPoints[bestInlierIndices[0]].x;
        double maxSpanCoordinate = projectAlongYAxis ? laserPoints[bestInlierIndices[bestCandidateInlierCount - 1]].y : laserPoints[bestInlierIndices[bestCandidateInlierCount - 1]].x;
        double segmentSpanLength = maxSpanCoordinate - minSpanCoordinate;

        // Duvar olamayacak kadar kisa gurultu kumelerini eler (< 22 cm)
        if (segmentSpanLength < MIN_WALL_LENGTH)
        {
            for (int j = 0; j < bestCandidateInlierCount; j++) pointOwnerLineIndex[bestInlierIndices[j]] = 999;
            continue;
        }

        if (bestCandidateLine.isVertical)
        {
            bestCandidateLine.startPoint.x = bestCandidateLine.endPoint.x = bestCandidateLine.verticalX;
            bestCandidateLine.startPoint.y = minSpanCoordinate;
            bestCandidateLine.endPoint.y   = maxSpanCoordinate;
        }
        else if (projectAlongYAxis)
        {
            bestCandidateLine.startPoint.y = minSpanCoordinate;
            bestCandidateLine.endPoint.y   = maxSpanCoordinate;
            bestCandidateLine.startPoint.x = (minSpanCoordinate - bestCandidateLine.intercept) / bestCandidateLine.slope;
            bestCandidateLine.endPoint.x   = (maxSpanCoordinate - bestCandidateLine.intercept) / bestCandidateLine.slope;
        }
        else
        {
            bestCandidateLine.startPoint.x = minSpanCoordinate;
            bestCandidateLine.endPoint.x   = maxSpanCoordinate;
            bestCandidateLine.startPoint.y = bestCandidateLine.slope * minSpanCoordinate + bestCandidateLine.intercept;
            bestCandidateLine.endPoint.y   = bestCandidateLine.slope * maxSpanCoordinate + bestCandidateLine.intercept;
        }

        bestCandidateLine.inlierCount = bestCandidateInlierCount;
        bestCandidateLine.inlierIndices = (int *)malloc(sizeof(int) * bestCandidateInlierCount);
        memcpy(bestCandidateLine.inlierIndices, bestInlierIndices, sizeof(int) * bestCandidateInlierCount);

        for (int j = 0; j < bestCandidateInlierCount; j++)
        {
            pointOwnerLineIndex[bestInlierIndices[j]] = detectedLineCount;
        }

        detectedLines[detectedLineCount++] = bestCandidateLine;
        consecutiveFailedAttempts = 0;
    }

    free(pointOwnerLineIndex);
    free(bestInlierIndices);
    free(candidateInlierIndices);

    *extractedLineCount = detectedLineCount;
    return detectedLines;
}

/* Noktanin dogru parcasinin sinir kutusu veya uc noktalarina yakinligini test eder. */
int isPointNearLineSegment(Point queryPoint, const Line *targetLine, double toleranceDistance)
{
    double boundingBoxMinX = fmin(targetLine->startPoint.x, targetLine->endPoint.x) - toleranceDistance;
    double boundingBoxMaxX = fmax(targetLine->startPoint.x, targetLine->endPoint.x) + toleranceDistance;
    double boundingBoxMinY = fmin(targetLine->startPoint.y, targetLine->endPoint.y) - toleranceDistance;
    double boundingBoxMaxY = fmax(targetLine->startPoint.y, targetLine->endPoint.y) + toleranceDistance;

    if (queryPoint.x >= boundingBoxMinX && queryPoint.x <= boundingBoxMaxX &&
        queryPoint.y >= boundingBoxMinY && queryPoint.y <= boundingBoxMaxY)
    {
        return 1;
    }

    double distanceToStart = hypot(queryPoint.x - targetLine->startPoint.x, queryPoint.y - targetLine->startPoint.y);
    double distanceToEnd   = hypot(queryPoint.x - targetLine->endPoint.x,   queryPoint.y - targetLine->endPoint.y);
    return (distanceToStart <= toleranceDistance || distanceToEnd <= toleranceDistance);
}

/* Iki dogrunun analitik kesisim noktasini hesaplar (paralelse 0 doner). */
int calculateLineIntersection(const Line *firstLine, const Line *secondLine, Point *intersectionPointOutput)
{
    if (firstLine->isVertical && secondLine->isVertical) return 0;

    if (firstLine->isVertical)
    {
        intersectionPointOutput->x = firstLine->verticalX;
        intersectionPointOutput->y = secondLine->slope * intersectionPointOutput->x + secondLine->intercept;
        return 1;
    }
    if (secondLine->isVertical)
    {
        intersectionPointOutput->x = secondLine->verticalX;
        intersectionPointOutput->y = firstLine->slope * intersectionPointOutput->x + firstLine->intercept;
        return 1;
    }

    double slopeDifference = firstLine->slope - secondLine->slope;
    if (fabs(slopeDifference) < 1e-9) return 0;

    intersectionPointOutput->x = (secondLine->intercept - firstLine->intercept) / slopeDifference;
    intersectionPointOutput->y = firstLine->slope * intersectionPointOutput->x + firstLine->intercept;
    return 1;
}

/* Iki dogru segmenti arasindaki dar/dik aciyi derece [0, 90] cinsinden hesaplar. */
double calculateAngleBetweenLinesDegrees(const Line *firstLine, const Line *secondLine)
{
    double directionX1 = firstLine->endPoint.x - firstLine->startPoint.x;
    double directionY1 = firstLine->endPoint.y - firstLine->startPoint.y;
    double directionX2 = secondLine->endPoint.x - secondLine->startPoint.x;
    double directionY2 = secondLine->endPoint.y - secondLine->startPoint.y;

    double dotProduct = directionX1 * directionX2 + directionY1 * directionY2;
    double vectorMagnitude1 = hypot(directionX1, directionY1);
    double vectorMagnitude2 = hypot(directionX2, directionY2);

    if (vectorMagnitude1 < 1e-9 || vectorMagnitude2 < 1e-9) return 0.0;

    // Nokta carpim egim isaretinden bagimsiz, sayisal olarak kararli dar aci verir
    double cosineTheta = fabs(dotProduct) / (vectorMagnitude1 * vectorMagnitude2);
    if (cosineTheta > 1.0) cosineTheta = 1.0;

    return acos(cosineTheta) * 180.0 / M_PI;
}

/* Belirli bir esik acisini asan ve fiziksel segment uclarinda gerceklesen kesisimleri bulur. */
Intersection *computeDockingIntersections(Line *detectedLines, int lineCount, double minAngleThresholdDegrees, int *intersectionCountOutput)
{
    if (lineCount < 2)
    {
        *intersectionCountOutput = 0;
        return NULL;
    }

    Intersection *intersectionArray = malloc(sizeof(Intersection) * lineCount * (lineCount - 1) / 2);
    if (!intersectionArray)
    {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    int validIntersectionCount = 0;
    for (int i = 0; i < lineCount; i++)
    {
        for (int j = i + 1; j < lineCount; j++)
        {
            double pairAngleDegrees = calculateAngleBetweenLinesDegrees(&detectedLines[i], &detectedLines[j]);
            if (pairAngleDegrees >= minAngleThresholdDegrees)
            {
                Point intersectionPoint;
                if (calculateLineIntersection(&detectedLines[i], &detectedLines[j], &intersectionPoint))
                {
                    if (isPointNearLineSegment(intersectionPoint, &detectedLines[i], 0.25) &&
                        isPointNearLineSegment(intersectionPoint, &detectedLines[j], 0.25))
                    {
                        intersectionArray[validIntersectionCount].coordinate = intersectionPoint;
                        intersectionArray[validIntersectionCount].distanceToRobot = hypot(intersectionPoint.x, intersectionPoint.y);
                        intersectionArray[validIntersectionCount].angleDegrees = pairAngleDegrees;
                        intersectionArray[validIntersectionCount].firstLineIndex = i + 1;
                        intersectionArray[validIntersectionCount].secondLineIndex = j + 1;
                        validIntersectionCount++;
                    }
                }
            }
        }
    }

    *intersectionCountOutput = validIntersectionCount;
    return intersectionArray;
}

#ifdef _WIN32
static BOOL CALLBACK refreshChildWindowProc(HWND childHandle, LPARAM lParam)
{
    (void)lParam;
    PostMessageA(childHandle, WM_MOUSEMOVE, 0, MAKELPARAM(100, 100));
    PostMessageA(childHandle, WM_MOUSEMOVE, 0, MAKELPARAM(105, 105));
    InvalidateRect(childHandle, NULL, TRUE);
    UpdateWindow(childHandle);
    return TRUE;
}

/* Windows uzerinde Gnuplot penceresini tam ekrana buyutmek icin arkaplan is parcacigi */
static DWORD WINAPI maximizeGnuplotWindowWorker(LPVOID threadParameter)
{
    (void)threadParameter;
    const char *targetTitle = "LIDAR RANSAC Line Detection and Docking";
    for (int attempt = 0; attempt < 50; attempt++)
    {
        Sleep(100);
        HWND gnuplotWindowHandle = FindWindowA(NULL, targetTitle);
        if (!gnuplotWindowHandle) gnuplotWindowHandle = FindWindowA(NULL, "Gnuplot (window id : 0)");
        if (gnuplotWindowHandle)
        {
            ShowWindow(gnuplotWindowHandle, SW_MAXIMIZE);
            Sleep(50);
            EnumChildWindows(gnuplotWindowHandle, refreshChildWindowProc, 0);
            PostMessageA(gnuplotWindowHandle, WM_MOUSEMOVE, 0, MAKELPARAM(100, 100));
            PostMessageA(gnuplotWindowHandle, WM_MOUSEMOVE, 0, MAKELPARAM(105, 105));
            InvalidateRect(gnuplotWindowHandle, NULL, TRUE);
            UpdateWindow(gnuplotWindowHandle);
            break;
        }
    }
    return 0;
}
#endif

/* Noktalari, dogrulari ve sarj istasyonu kesisimini Gnuplot ile gorsellestirir. */
void renderVisualizationWithGnuplot(Point *laserPoints, int totalPoints, Line *detectedLines, int lineCount, Intersection *intersections, int intersectionCount, int isEnglish)
{
    FILE *gnuplotPipeHandle = popen("gnuplot -persistent", "w");
    if (!gnuplotPipeHandle)
    {
        fprintf(stderr, isEnglish ? "Failed to open gnuplot\n" : "gnuplot acilamadi\n");
        return;
    }
    fprintf(gnuplotPipeHandle, "set encoding utf8\n");

#ifdef _WIN32
    RECT desktopWorkArea;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &desktopWorkArea, 0);
    int screenWidth  = desktopWorkArea.right - desktopWorkArea.left;
    int screenHeight = desktopWorkArea.bottom - desktopWorkArea.top;
    fprintf(gnuplotPipeHandle, "set terminal wxt 0 title 'LIDAR RANSAC Line Detection and Docking' size %d,%d position 0,0 enhanced persist background rgb 'white'\n",
            screenWidth > 0 ? screenWidth : 1600, screenHeight > 0 ? screenHeight : 900);
    CreateThread(NULL, 0, maximizeGnuplotWindowWorker, NULL, 0, NULL);
#else
    fprintf(gnuplotPipeHandle, "set terminal wxt size 1600,900 position 50,50 enhanced persist background rgb 'white'\n");
#endif

    fprintf(gnuplotPipeHandle, "set object 999 rectangle from screen 0,0 to screen 1,1 fillcolor rgb 'white' fillstyle solid 1.0 noborder behind\n");
    fprintf(gnuplotPipeHandle, "set datafile separator ' '\nset decimalsign '.'\n");
    fprintf(gnuplotPipeHandle, "set grid lc rgb '#dddddd' lw 1\nset key outside right top font ',9'\nset xlabel 'X (m)'\nset ylabel 'Y (m)'\n");
    if (isEnglish)
    {
        fprintf(gnuplotPipeHandle, "set title \"LIDAR RANSAC Line Detection and Docking\\n{/*0.85 {/:Italic [Arrow Keys]: Pan  |  [Ctrl + Wheel]: Zoom  |  [a] Key: 1:1 Reset / Fit All}}\"\n");
    }
    else
    {
        fprintf(gnuplotPipeHandle, "set title \"LIDAR RANSAC Doğru Tespiti ve Şarj İstasyonu Kesişimi\\n{/*0.85 {/:Italic [Yön Tuşları]: Gezin  |  [Ctrl + Tekerlek]: Yakınlaştır  |  [a] Tuşu: 1:1 Merkeze Dön / Sıfırla}}\"\n");
    }

    fprintf(gnuplotPipeHandle, "set style textbox 1 opaque fillcolor rgb '#fff176' border lc rgb '#f57f17' lw 2\n");
    fprintf(gnuplotPipeHandle, "set style textbox 2 opaque fillcolor rgb '#e8f5e9' border lc rgb '#2e7d32' lw 1.5\n");
    fprintf(gnuplotPipeHandle, "set style textbox 3 opaque fillcolor rgb '#ffffff' border lc rgb '#2e7d32' lw 1\n");

    int bestIntersectionIndex = -1;
    if (intersectionCount > 0)
    {
        double lowestCandidateScore = 1e9;
        for (int i = 0; i < intersectionCount; i++)
        {
            double angleDeviation = fabs(intersections[i].angleDegrees - 90.0);
            double candidateScore = angleDeviation * 0.1 + intersections[i].distanceToRobot;
            if (candidateScore < lowestCandidateScore)
            {
                lowestCandidateScore = candidateScore;
                bestIntersectionIndex = i;
            }
        }
    }

    double minBoundingX = 0.0, maxBoundingX = 0.0;
    double minBoundingY = 0.0, maxBoundingY = 0.0;

    for (int i = 0; i < totalPoints; i++)
    {
        if (laserPoints[i].x < minBoundingX) minBoundingX = laserPoints[i].x;
        if (laserPoints[i].x > maxBoundingX) maxBoundingX = laserPoints[i].x;
        if (laserPoints[i].y < minBoundingY) minBoundingY = laserPoints[i].y;
        if (laserPoints[i].y > maxBoundingY) maxBoundingY = laserPoints[i].y;
    }

    if (bestIntersectionIndex >= 0)
    {
        if (intersections[bestIntersectionIndex].coordinate.x < minBoundingX) minBoundingX = intersections[bestIntersectionIndex].coordinate.x;
        if (intersections[bestIntersectionIndex].coordinate.x > maxBoundingX) maxBoundingX = intersections[bestIntersectionIndex].coordinate.x;
        if (intersections[bestIntersectionIndex].coordinate.y < minBoundingY) minBoundingY = intersections[bestIntersectionIndex].coordinate.y;
        if (intersections[bestIntersectionIndex].coordinate.y > maxBoundingY) maxBoundingY = intersections[bestIntersectionIndex].coordinate.y;
    }

    double marginPadding = 0.40;
    minBoundingX -= marginPadding; maxBoundingX += marginPadding;
    minBoundingY -= marginPadding; maxBoundingY += marginPadding;

    double spanX = maxBoundingX - minBoundingX;
    double spanY = maxBoundingY - minBoundingY;
    double maximumSpan = fmax(spanX, spanY);
    double centerX = (minBoundingX + maxBoundingX) / 2.0;
    double centerY = (minBoundingY + maxBoundingY) / 2.0;

    minBoundingX = centerX - maximumSpan / 2.0;
    maxBoundingX = centerX + maximumSpan / 2.0;
    minBoundingY = centerY - maximumSpan / 2.0;
    maxBoundingY = centerY + maximumSpan / 2.0;

    fprintf(gnuplotPipeHandle, "set size ratio -1\n");
    fprintf(gnuplotPipeHandle, "set xrange [%f:%f]\n", minBoundingX, maxBoundingX);
    fprintf(gnuplotPipeHandle, "set yrange [%f:%f]\n", minBoundingY, maxBoundingY);

    fprintf(gnuplotPipeHandle, "$robot << EOD\n0 0\nEOD\n$pts << EOD\n");
    for (int i = 0; i < totalPoints; i++)
    {
        fprintf(gnuplotPipeHandle, "%f %f\n", laserPoints[i].x, laserPoints[i].y);
    }
    fprintf(gnuplotPipeHandle, "EOD\n$ints << EOD\n");
    for (int i = 0; i < intersectionCount; i++)
    {
        fprintf(gnuplotPipeHandle, "%f %f\n", intersections[i].coordinate.x, intersections[i].coordinate.y);
    }
    fprintf(gnuplotPipeHandle, "EOD\n");

    if (bestIntersectionIndex >= 0)
    {
        fprintf(gnuplotPipeHandle, "$dist_line << EOD\n0 0\n%f %f\nEOD\n",
                intersections[bestIntersectionIndex].coordinate.x, intersections[bestIntersectionIndex].coordinate.y);
        fprintf(gnuplotPipeHandle, "$dist_label << EOD\n%f %f %.2fm\nEOD\n",
                intersections[bestIntersectionIndex].coordinate.x / 2.0,
                intersections[bestIntersectionIndex].coordinate.y / 2.0,
                intersections[bestIntersectionIndex].distanceToRobot);
        fprintf(gnuplotPipeHandle, "$station << EOD\n%f %f\nEOD\n",
                intersections[bestIntersectionIndex].coordinate.x, intersections[bestIntersectionIndex].coordinate.y);
        fprintf(gnuplotPipeHandle, "$station_label << EOD\n%f %f \"%.0f°\\n(d%d ∩ d%d)\"\nEOD\n",
                intersections[bestIntersectionIndex].coordinate.x,
                intersections[bestIntersectionIndex].coordinate.y,
                intersections[bestIntersectionIndex].angleDegrees,
                intersections[bestIntersectionIndex].firstLineIndex,
                intersections[bestIntersectionIndex].secondLineIndex);
    }

    fprintf(gnuplotPipeHandle, "$d_labels << EOD\n");
    for (int i = 0; i < lineCount; i++)
    {
        double midpointX = (detectedLines[i].startPoint.x + detectedLines[i].endPoint.x) / 2.0;
        double midpointY = (detectedLines[i].startPoint.y + detectedLines[i].endPoint.y) / 2.0;
        fprintf(gnuplotPipeHandle, "%f %f d%d\n", midpointX, midpointY, i + 1);
    }
    fprintf(gnuplotPipeHandle, "EOD\n");

    for (int i = 0; i < lineCount; i++)
    {
        fprintf(gnuplotPipeHandle, "$line%d << EOD\n%f %f\n%f %f\nEOD\n", i,
                detectedLines[i].startPoint.x, detectedLines[i].startPoint.y,
                detectedLines[i].endPoint.x,   detectedLines[i].endPoint.y);
        fprintf(gnuplotPipeHandle, "$inlier%d << EOD\n", i);
        for (int j = 0; j < detectedLines[i].inlierCount; j++)
        {
            int pointIdx = detectedLines[i].inlierIndices[j];
            fprintf(gnuplotPipeHandle, "%f %f\n", laserPoints[pointIdx].x, laserPoints[pointIdx].y);
        }
        fprintf(gnuplotPipeHandle, "EOD\n");
    }

    fprintf(gnuplotPipeHandle, "plot \\\n"
            "$pts using 1:2 with points pt 7 ps 0.4 lc rgb '#aaaaaa' title '%s', \\\n"
            "$robot using 1:2 with points pt 7 ps 2 lc rgb 'red' title 'Robot (0,0)'",
            isEnglish ? "All Points" : "Tüm Noktalar");

    if (bestIntersectionIndex >= 0)
    {
        fprintf(gnuplotPipeHandle, ", \\\n$dist_line using 1:2 with lines dt 2 lw 3 lc rgb 'red' notitle");
        fprintf(gnuplotPipeHandle, ", \\\n$dist_label using 1:2:3 with labels boxed bs 1 font ',10,Bold' tc rgb 'black' notitle");
        fprintf(gnuplotPipeHandle, ", \\\n$station using 1:2 with points pt 7 ps 2.2 lc rgb '#ff7f00' title '%s'",
                isEnglish ? "60°+ Docking Apex" : "60°+ Kesişim");
        fprintf(gnuplotPipeHandle, ", \\\n$station_label using 1:2:3 with labels boxed bs 2 offset 0,1.3 font ',9,Bold' tc rgb '#1b5e20' notitle");
    }

    for (int i = 0; i < lineCount; i++)
    {
        fprintf(gnuplotPipeHandle, ", \\\n$inlier%d using 1:2 with points pt 7 ps 0.7 lc rgb '#1b7837' title '%s'",
                i, isEnglish ? "inliers" : "noktaları");
        fprintf(gnuplotPipeHandle, ", \\\n$line%d using 1:2 with lines lw 2.5 lc rgb '#00441b' title '%s'",
                i, detectedLines[i].label);
    }

    fprintf(gnuplotPipeHandle, ", \\\n$d_labels using 1:2:3 with labels boxed bs 3 font ',9,Bold' tc rgb '#004d40' notitle");
    fprintf(gnuplotPipeHandle, "\n");

    fprintf(gnuplotPipeHandle, "pause -1 \"%s\\n\"\n",
            isEnglish ? "Press Enter to close plot... (Navigation: Arrow Keys = Pan, Ctrl+Wheel = Zoom, 'a' = 1:1 Reset)"
                      : "Grafiği kapatmak için Enter'a basın... (Gezinme: Yön Tuşları = Kaydır, Ctrl+Tekerlek = Yakınlaştır, 'a' = 1:1 Sıfırla)");
    fflush(gnuplotPipeHandle);
    pclose(gnuplotPipeHandle);
}

int main(int argumentCount, char **argumentVector)
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    curl_global_init(CURL_GLOBAL_ALL);
    setlocale(LC_NUMERIC, "C");

    int isEnglish = 1; // Default is English
    const char *targetUrlOrFilePath = NULL;
    const char *defaultScanUrl = "http://abilgisayar.kocaeli.edu.tr/lidar1.toml";
    const char *activeTomlFileName;
    const char *temporaryDownloadedFileName = "downloaded_lidar.toml";
    char userInputBuffer[1024];

    // Komut satiri argumanlarini tara (ornek: --tr, --en veya dosya yolu)
    for (int i = 1; i < argumentCount; i++)
    {
        if (strcmp(argumentVector[i], "--tr") == 0 || strcmp(argumentVector[i], "-tr") == 0)
        {
            isEnglish = 0;
        }
        else if (strcmp(argumentVector[i], "--en") == 0 || strcmp(argumentVector[i], "-en") == 0)
        {
            isEnglish = 1;
        }
        else if (argumentVector[i][0] != '-')
        {
            targetUrlOrFilePath = argumentVector[i];
        }
    }

    if (targetUrlOrFilePath != NULL)
    {
        printf(isEnglish ? "CLI argument detected: %s\n" : "Komut satırı argümanı algılandı: %s\n", targetUrlOrFilePath);
    }
    else
    {
        printf("================================================================================\n");
        printf("        LIDAR RANSAC Line Detection and Docking / Şarj İstasyonu Tespiti        \n");
        printf("================================================================================\n");
        printf("Language / Dil:\n");
        printf("  English  : [ENTER]\n");
        printf("  Türkçe   : [2'ye bas + ENTER]\n> ");
        char langBuffer[32];
        if (fgets(langBuffer, sizeof(langBuffer), stdin) != NULL)
        {
            trimWhitespaceAndQuotes(langBuffer);
            if (langBuffer[0] == '2' || langBuffer[0] == 't' || langBuffer[0] == 'T')
            {
                isEnglish = 0; // Turkce secildi
            }
            else
            {
                isEnglish = 1; // ENTER veya diger tuslar: varsayilan Ingilizce
            }
        }

        if (isEnglish)
        {
            printf("\nEnter TOML file name, path, URL, or drag and drop: [ENTER = default]\n> ");
        }
        else
        {
            printf("\nTOML dosya adı, dosya yolu veya URL girin ya da sürükleyip bırakın: [ENTER = varsayılan]\n> ");
        }

        if (fgets(userInputBuffer, sizeof(userInputBuffer), stdin) == NULL)
        {
            fprintf(stderr, isEnglish ? "Input error.\n" : "Girdi okuma hatası.\n");
            curl_global_cleanup();
            return 1;
        }
        trimWhitespaceAndQuotes(userInputBuffer);
        if (strlen(userInputBuffer) == 0)
        {
            printf(isEnglish ? "No input given, using default URL: %s\n" : "Girdi boş, varsayılan URL kullanılıyor: %s\n", defaultScanUrl);
            targetUrlOrFilePath = defaultScanUrl;
        }
        else
        {
            targetUrlOrFilePath = userInputBuffer;
        }
    }

    if (strncmp(targetUrlOrFilePath, "http", 4) == 0)
    {
        printf(isEnglish ? "URL detected, downloading: %s\n" : "URL algılandı, indiriliyor: %s\n", targetUrlOrFilePath);
        int downloadSuccess = downloadRemoteFile(targetUrlOrFilePath, temporaryDownloadedFileName);
        if (!downloadSuccess)
        {
            fprintf(stderr, isEnglish ? "Failed to download file. Aborting.\n" : "Dosya indirilemedi. Program durduruluyor.\n");
            curl_global_cleanup();
            return 1;
        }
        activeTomlFileName = temporaryDownloadedFileName;
        printf(isEnglish ? "Download successful, saved to %s.\n" : "İndirme başarılı, %s dosyasına kaydedildi.\n", activeTomlFileName);
    }
    else
    {
        printf(isEnglish ? "Local file detected: %s\n" : "Yerel dosya algılandı: %s\n", targetUrlOrFilePath);
        activeTomlFileName = targetUrlOrFilePath;
    }

    LidarScan scanData = parseTomlScanFile(activeTomlFileName);
    if (isEnglish)
    {
        printf("\n=== TOML Header ===\n");
        printf("  Timestamp       : %s\n", scanData.timestamp[0] ? scanData.timestamp : "(none)");
        printf("  Reference Frame : %s\n", scanData.frameId[0] ? scanData.frameId : "(none)");
        printf("  angle_min=%.4f  angle_max=%.4f  angle_increment=%.6f\n", scanData.angleMin, scanData.angleMax, scanData.angleIncrement);
        printf("  range_min=%.2f  range_max=%.2f  total measurements=%d\n", scanData.rangeMin, scanData.rangeMax, scanData.rangeCount);
        printf("====================\n");
    }
    else
    {
        printf("\n=== TOML Başlığı ===\n");
        printf("  Zaman Damgası   : %s\n", scanData.timestamp[0] ? scanData.timestamp : "(yok)");
        printf("  Referans Çerçeve: %s\n", scanData.frameId[0] ? scanData.frameId : "(yok)");
        printf("  angle_min=%.4f  angle_max=%.4f  angle_increment=%.6f\n", scanData.angleMin, scanData.angleMax, scanData.angleIncrement);
        printf("  range_min=%.2f  range_max=%.2f  toplam ölçüm=%d\n", scanData.rangeMin, scanData.rangeMax, scanData.rangeCount);
        printf("====================\n");
    }

    filterInvalidLaserRanges(&scanData);

    int cartesianPointCount;
    Point *cartesianPoints = convertPolarToCartesian(&scanData, &cartesianPointCount);

    if (cartesianPointCount < 8)
    {
        fprintf(stderr, isEnglish ? "Insufficient valid points (%d). Aborting.\n" : "Yeterli geçerli nokta bulunamadı (%d). Program durduruluyor.\n", cartesianPointCount);
        free(cartesianPoints);
        free(scanData.rangeMeasurements);
        curl_global_cleanup();
        return 1;
    }

    int detectedLineCount;
    Line *detectedLines = extractWallsUsingRANSAC(cartesianPoints, cartesianPointCount, &detectedLineCount);
    printf(isEnglish ? "RANSAC extracted %d linear segments.\n" : "RANSAC %d doğrusal segment buldu.\n", detectedLineCount);

    for (int i = 0; i < detectedLineCount; i++)
    {
        double segmentLength = hypot(detectedLines[i].endPoint.x - detectedLines[i].startPoint.x,
                                     detectedLines[i].endPoint.y - detectedLines[i].startPoint.y);
        sprintf(detectedLines[i].label, "d%d (%.2fm)", i + 1, segmentLength);
    }

    int dockingIntersectionCount;
    Intersection *dockingIntersections = computeDockingIntersections(detectedLines, detectedLineCount, 60.0, &dockingIntersectionCount);
    if (isEnglish)
    {
        printf("Found %d docking intersections with angle >= 60 deg:\n", dockingIntersectionCount);
        for (int i = 0; i < dockingIntersectionCount; i++)
        {
            printf("Int %d (d%d & d%d): (%.3f, %.3f) dist=%.2fm angle=%.1f deg\n",
                   i + 1, dockingIntersections[i].firstLineIndex, dockingIntersections[i].secondLineIndex,
                   dockingIntersections[i].coordinate.x, dockingIntersections[i].coordinate.y,
                   dockingIntersections[i].distanceToRobot, dockingIntersections[i].angleDegrees);
        }
    }
    else
    {
        printf("Açısı >= 60 derece olan %d şarj kesişimi bulundu:\n", dockingIntersectionCount);
        for (int i = 0; i < dockingIntersectionCount; i++)
        {
            printf("Kesişim %d (d%d & d%d): (%.3f, %.3f) mesafe=%.2fm açı=%.1f derece\n",
                   i + 1, dockingIntersections[i].firstLineIndex, dockingIntersections[i].secondLineIndex,
                   dockingIntersections[i].coordinate.x, dockingIntersections[i].coordinate.y,
                   dockingIntersections[i].distanceToRobot, dockingIntersections[i].angleDegrees);
        }
    }
    fflush(stdout);

    renderVisualizationWithGnuplot(cartesianPoints, cartesianPointCount, detectedLines, detectedLineCount, dockingIntersections, dockingIntersectionCount, isEnglish);

    free(cartesianPoints);
    free(scanData.rangeMeasurements);
    for (int i = 0; i < detectedLineCount; i++) free(detectedLines[i].inlierIndices);
    free(detectedLines);
    free(dockingIntersections);
    curl_global_cleanup();
    return 0;
}
