#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

int StrLen(const char* text) { // вычисл длины строки
    int length = 0;
    while (text[length]) {
        length++;
    }
    return length;
}

void IntToStr(int value, char* buffer) { // числа в строку
    char temp[20];
    int tempIndex = 0;
    int i = 0;
    int bufferIndex = 0;
    
    if (value == 0) {
        buffer[bufferIndex++] = '0';
    } else {
        while (value > 0) {
            temp[tempIndex++] = '0' + (value % 10);
            value /= 10;
        }
        
        for (i = tempIndex - 1; i >= 0; i--) {
            buffer[bufferIndex++] = temp[i];
        }
    }
    buffer[bufferIndex] = '\0';
}

void LongLongToStr(long long value, char* buffer) {
    char temp[30];
    int tempIndex = 0;
    int i = 0;
    int bufferIndex = 0;
    if (value == 0) {
        buffer[bufferIndex++] = '0';
    } else {
        while (value > 0) {
            temp[tempIndex++] = '0' + (value % 10);
            value /= 10;
        }
        
        for (i = tempIndex - 1; i >= 0; i--) {
            buffer[bufferIndex++] = temp[i];
        }
    }
    buffer[bufferIndex] = '\0';
}

typedef struct { // стр для передачи данных в поток
    const char* text;
    const char* pattern;
    int textLen;
    int patternLen;
    int startIdx;
    int endIdx;
    int* matches;
    int matchCount;
    int maxMatchesPerThread;
} TThreadData;

void* Malloc(size_t size) { // аллокация памяти
    void* result = sbrk(size);
    if (result == (void*)-1) {
        return NULL;
    }
    return result;
}

int SequentialSearch(const char* text, const char* pattern, int* matches) {
    const int textLength = StrLen(text);
    const int patternLength = StrLen(pattern);
    int count = 0;
    for (int i = 0; i <= textLength - patternLength; i++) {
        int j;
        for (j = 0; j < patternLength; j++) {
            if (text[i + j] != pattern[j]) {
                break;
            }
        }
        if (j == patternLength) {
            matches[count] = i;
            count++;
        }
    }
    return count;
}

void* ThreadSearch(void* arg) { // для потока в паралл.
    TThreadData* data = (TThreadData*)arg;
    data->matchCount = 0;
    int actualEndIdx = data->endIdx;
    if (actualEndIdx + data->patternLen > data->textLen) {
        actualEndIdx = data->textLen - data->patternLen;
    }
    for (int i = data->startIdx; i <= actualEndIdx; i++) {
        if (data->matchCount >= data->maxMatchesPerThread) {
            break;
        }
        int j;
        for (j = 0; j < data->patternLen; j++) {
            if (data->text[i + j] != data->pattern[j]) {
                break;
            }
        }
        if (j == data->patternLen) {
            data->matches[data->matchCount] = i;
            data->matchCount++;
        }
    }
    return NULL;
}

int ParallelSearch(const char* text, const char* pattern, int* finalMatches, int numThreads) {
    const int textLength = StrLen(text);
    const int patternLength = StrLen(pattern);
    const int totalPositions = textLength - patternLength + 1;
    if (totalPositions <= 0) {
        return 0;
    }
    if (numThreads == 1) { // для 1 потока последовательный поиск
        return SequentialSearch(text, pattern, finalMatches); 
    }
    pthread_t* threads = (pthread_t*)Malloc(numThreads * sizeof(pthread_t));
    TThreadData* threadData = (TThreadData*)Malloc(numThreads * sizeof(TThreadData));
    
    if (threads == NULL || threadData == NULL) {
        return 0;
    }
    
    const int baseChunk = totalPositions / numThreads;
    const int remainder = totalPositions % numThreads;
    int currentStart = 0;
    for (int i = 0; i < numThreads; i++) { // память для каждого потока
        int chunkSize = baseChunk + (i < remainder ? 1 : 0);
        int endIdx = currentStart + chunkSize - 1;
        if (i < numThreads - 1) {
            endIdx += patternLength - 1;
        }
        
        if (endIdx >= totalPositions) {
            endIdx = totalPositions - 1;
        }
        int maxMatchesForThread = chunkSize + patternLength;
        int* threadMatches = (int*)Malloc(maxMatchesForThread * sizeof(int));
        if (threadMatches == NULL) {
            return 0;
        }
        threadData[i].text = text;
        threadData[i].pattern = pattern;
        threadData[i].textLen = textLength;
        threadData[i].patternLen = patternLength;
        threadData[i].startIdx = currentStart;
        threadData[i].endIdx = endIdx;
        threadData[i].matches = threadMatches;
        threadData[i].matchCount = 0;
        threadData[i].maxMatchesPerThread = maxMatchesForThread;
        currentStart += chunkSize;
    }
    for (int i = 0; i < numThreads; i++) { // запуск потоков
        pthread_create(&threads[i], NULL, ThreadSearch, &threadData[i]);
    }
    int totalMatches = 0; // результаты
    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
        for (int j = 0; j < threadData[i].matchCount; j++) { // копируем резы потока в конечный массив
            int isDuplicate = 0;
            for (int k = 0; k < totalMatches; k++) {
                if (finalMatches[k] == threadData[i].matches[j]) {
                    isDuplicate = 1;
                    break;
                }
            }
            if (!isDuplicate) {
                finalMatches[totalMatches] = threadData[i].matches[j];
                totalMatches++;
            }
        }
    }
    
    return totalMatches;
}

long long GetTimeMs() {
    struct timeval timeValue;
    gettimeofday(&timeValue, NULL);
    return (long long)timeValue.tv_sec * 1000 + timeValue.tv_usec / 1000;
}

void ReadFile(const char* filename, char** buffer, int* length) {
    const int fileDescriptor = open(filename, O_RDONLY);
    if (fileDescriptor < 0) {
        *length = 0;
        *buffer = NULL;
        return;
    }
    struct stat fileStat;
    fstat(fileDescriptor, &fileStat);
    *length = fileStat.st_size;
    *buffer = (char*)Malloc(*length + 1);
    if (*buffer != NULL) {
        read(fileDescriptor, *buffer, *length);
        (*buffer)[*length] = '\0';
    }
    close(fileDescriptor);
}

int main(int argc, char* argv[]) {
    const int MIN_ARGUMENTS = 4;
    if (argc < MIN_ARGUMENTS) {
        const char* errorMessage = "Usage: ./program <pattern> <file> <max_threads>\n";
        write(STDOUT_FILENO, errorMessage, StrLen(errorMessage));
        return 1;
    }
    const char* pattern = argv[1];
    const char* filename = argv[2];
    int maxThreads = 0;
    for (int i = 0; argv[3][i]; i++) {
        maxThreads = maxThreads * 10 + (argv[3][i] - '0');
    }
    if (maxThreads <= 0) {
        const char* errorMessage = "Error: max_threads must be positive\n";
        write(STDOUT_FILENO, errorMessage, StrLen(errorMessage));
        return 1;
    }
    char* text; // Читаем файл
    int textLength;
    ReadFile(filename, &text, &textLength);
    if (text == NULL) {
        const char* errorMessage = "Error: Cannot open file or allocate memory\n";
        write(STDOUT_FILENO, errorMessage, StrLen(errorMessage));
        return 1;
    }
    
    char infoBuffer[128]; // информация о файле
    int infoIndex = 0;
    const char* infoStr = "File size: ";
    for (int i = 0; infoStr[i]; i++) infoBuffer[infoIndex++] = infoStr[i];
    char sizeStr[20];
    IntToStr(textLength, sizeStr);
    for (int i = 0; sizeStr[i]; i++) infoBuffer[infoIndex++] = sizeStr[i];
    const char* bytesStr = " bytes\n";
    for (int i = 0; bytesStr[i]; i++) infoBuffer[infoIndex++] = bytesStr[i];
    write(STDOUT_FILENO, infoBuffer, infoIndex);
    const int patternLength = StrLen(pattern);
    const int maxMatches = textLength - patternLength + 1;
    if (maxMatches <= 0) {
        const char* errorMessage = "Error: Pattern longer than text\n";
        write(STDOUT_FILENO, errorMessage, StrLen(errorMessage));
        return 1;
    }
    int* matches = (int*)Malloc(maxMatches * sizeof(int));
    if (matches == NULL) {
        const char* errorMessage = "Error: Cannot allocate memory for matches\n";
        write(STDOUT_FILENO, errorMessage, StrLen(errorMessage));
        return 1;
    }
    
    const long long startTimeSeq = GetTimeMs(); // последовательная
    int seqMatches = SequentialSearch(text, pattern, matches);
    const long long seqTime = GetTimeMs() - startTimeSeq;
    char seqBuffer[128]; // вывод
    int seqBufferIndex = 0;
    const char* seqTimeStr = "Sequential: ";
    for (int i = 0; seqTimeStr[i]; i++) seqBuffer[seqBufferIndex++] = seqTimeStr[i];
    char seqTimeNumStr[30];
    LongLongToStr(seqTime, seqTimeNumStr);
    for (int i = 0; seqTimeNumStr[i]; i++) seqBuffer[seqBufferIndex++] = seqTimeNumStr[i];
    const char* seqMatchesStr = " ms, matches: ";
    for (int i = 0; seqMatchesStr[i]; i++) seqBuffer[seqBufferIndex++] = seqMatchesStr[i];
    char seqMatchesNumStr[20];
    IntToStr(seqMatches, seqMatchesNumStr);
    for (int i = 0; seqMatchesNumStr[i]; i++) seqBuffer[seqBufferIndex++] = seqMatchesNumStr[i];
    const char* newlineStr = "\n";
    for (int i = 0; newlineStr[i]; i++) seqBuffer[seqBufferIndex++] = newlineStr[i];
    write(STDOUT_FILENO, seqBuffer, seqBufferIndex);
    
    for (int numThreads = 1; numThreads <= maxThreads; numThreads++) {
        const long long startTimePar = GetTimeMs();
        int parMatches = ParallelSearch(text, pattern, matches, numThreads);
        const long long parTime = GetTimeMs() - startTimePar;
        char buffer[256];
        int bufferIndex = 0;
        const char* threadsStr = "Threads: ";
        for (int i = 0; threadsStr[i]; i++) buffer[bufferIndex++] = threadsStr[i];
        char numStr[20];
        IntToStr(numThreads, numStr);
        for (int i = 0; numStr[i]; i++) buffer[bufferIndex++] = numStr[i];
        const char* timeStr = ", Time: ";
        for (int i = 0; timeStr[i]; i++) buffer[bufferIndex++] = timeStr[i];
        char timeNumStr[30];
        LongLongToStr(parTime, timeNumStr);
        for (int i = 0; timeNumStr[i]; i++) buffer[bufferIndex++] = timeNumStr[i];
        const char* matchesStr = " ms, Matches: ";
        for (int i = 0; matchesStr[i]; i++) buffer[bufferIndex++] = matchesStr[i];
        char matchesNumStr[20];
        IntToStr(parMatches, matchesNumStr);
        for (int i = 0; matchesNumStr[i]; i++) buffer[bufferIndex++] = matchesNumStr[i];
        const char* msStr = "\n";
        for (int i = 0; msStr[i]; i++) buffer[bufferIndex++] = msStr[i];
        write(STDOUT_FILENO, buffer, bufferIndex);
    }
    return 0;
}