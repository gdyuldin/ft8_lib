#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#include <ft8/decode.h>
#include <ft8/encode.h>
#include <ft8/message.h>
#include <ft8/hashtable.h>

#include <common/common.h>
#include <common/wave.h>
#include <common/monitor.h>
#include <common/audio.h>

#define LOG_LEVEL LOG_INFO
#include <ft8/debug.h>

const int kMin_score = 10; // Minimum sync score threshold for candidates
const int kMax_candidates = 200;
const int kLDPC_iterations = 25;

const int kMax_decoded_messages = 50;

const int kFreq_osr = 2; // Frequency oversampling rate (bin subdivision)
const int kTime_osr = 4; // Time oversampling rate (symbol subdivision)


static void swap(int* xp, int* yp) {
    int temp = *xp;
    *xp = *yp;
    *yp = temp;
}

// Function to perform Selection Sort
static void selectionSort(int arr[], int n) {
    int i, j, min_idx;

    for (i = 0; i < n-1; i++) {
        min_idx = i;

        for (j = i+1; j < n; j++)
          if (arr[j] < arr[min_idx])
            min_idx = j;

        swap(&arr[min_idx], &arr[i]);
    }
}


int ftx_get_snr(const ftx_waterfall_t* wf, const ftx_candidate_t *candidate) {
    //array with wf.num_blocks (row of watterfall) x 8*wf.freq_osr (signals witdh)
    //Get this waterfall zoom on the candidate symbols
    //Sort max to min and calculate max/min = ft8snr and return with substract of -26db for get snr on 2500hz

    int     m = wf->freq_osr * wf->time_osr;
    int     l = 8 * m;
    // int     n = 2 * m;
    float   minC = 0, maxC = 0;
    int     i = 0;
    int low_part = 2;
    int high_part = 1;

    while (i < wf->num_blocks) {
        int candidate_zoom[l];

        for (int j = 0; j< 8; j++) {
            for(int k = 0; k<m; k++) {
                candidate_zoom[(j*m)+k] = wf->mag[( (i * wf->block_stride) + candidate->freq_offset + candidate->freq_sub + (j*m) + k )];
            }
        }

        selectionSort(candidate_zoom, l);

        for (int j = 0; j < low_part * m; j++) {
            minC += candidate_zoom[j];
        }

        for (int j = l - 1; j >= l - (m * high_part); j--) {
            maxC += candidate_zoom[j];
        }

        i++;
    }

    minC = minC / (wf->num_blocks*wf->freq_osr*wf->time_osr*low_part);
    maxC = maxC / (wf->num_blocks*wf->freq_osr*wf->time_osr*high_part);

    int min = (int)(minC/2 - 240);
    int max = (int)(maxC/2 - 240);

    int snr= max - min - 26;

    return snr;
}


void usage(const char* error_msg)
{
    if (error_msg != NULL)
    {
        fprintf(stderr, "ERROR: %s\n", error_msg);
    }
    fprintf(stderr, "Usage: decode_ft8 [-list|([-ft4] [INPUT|-dev DEVICE])]\n\n");
    fprintf(stderr, "Decode a 15-second (or slighly shorter) WAV file.\n");
}

int find_candidates(const monitor_t* mon, ftx_candidate_t *candidate_list, int maxCandidates) {
    const ftx_waterfall_t* wf = &mon->wf;
    return ftx_find_candidates(wf, maxCandidates, candidate_list, kMin_score);
}

int decode_messages(const monitor_t* mon, int *num_candidates, ftx_candidate_t *candidate_list, ftx_message_t *decoded, ftx_message_t **decoded_hashtable, int ldpc_iterations, struct tm* tm_slot_start) {
    // Go over candidates and attempt to decode messages
    const ftx_waterfall_t* wf = &mon->wf;
    int to_delete_idx[*num_candidates];
    int to_delete_size = 0;
    int num_decoded = 0;

    for (int idx = 0; idx < *num_candidates; ++idx)
    {
        const ftx_candidate_t* cand = &candidate_list[idx];

        if ((cand->time_offset + 70) > wf->num_blocks) {
            continue;
        }

        ftx_message_t message;
        ftx_decode_status_t status;
        if (!ftx_decode_candidate(wf, cand, ldpc_iterations, &message, &status))
        {
            if (status.ldpc_errors > 0)
            {
                LOG(LOG_DEBUG, "LDPC decode: %d errors\n", status.ldpc_errors);
            }
            else if (status.crc_calculated != status.crc_extracted)
            {
                LOG(LOG_DEBUG, "CRC mismatch!\n");
                to_delete_idx[to_delete_size++] = idx;
            }
            continue;
        }
        to_delete_idx[to_delete_size++] = idx;

        float freq_hz = (mon->min_bin + cand->freq_offset + (float)cand->freq_sub / wf->freq_osr) / mon->symbol_period;
        float time_sec = (cand->time_offset + (float)cand->time_sub / wf->time_osr) * mon->symbol_period;

        LOG(LOG_DEBUG, "Checking hash table for %4.1fs / %4.1fHz [%d]...\n", time_sec, freq_hz, cand->score);
        int idx_hash = message.hash % kMax_decoded_messages;
        bool found_empty_slot = false;
        bool found_duplicate = false;
        do
        {
            if (decoded_hashtable[idx_hash] == NULL)
            {
                LOG(LOG_DEBUG, "Found an empty slot\n");
                found_empty_slot = true;
            }
            else if ((decoded_hashtable[idx_hash]->hash == message.hash) && (0 == memcmp(decoded_hashtable[idx_hash]->payload, message.payload, sizeof(message.payload))))
            {
                LOG(LOG_DEBUG, "Found a duplicate!\n");
                found_duplicate = true;
            }
            else
            {
                LOG(LOG_DEBUG, "Hash table clash!\n");
                // Move on to check the next entry in hash table
                idx_hash = (idx_hash + 1) % kMax_decoded_messages;
            }
        } while (!found_empty_slot && !found_duplicate);

        if (found_empty_slot)
        {
            // Fill the empty hashtable slot
            memcpy(&decoded[idx_hash], &message, sizeof(message));
            decoded_hashtable[idx_hash] = &decoded[idx_hash];

            char text[FTX_MAX_MESSAGE_LENGTH];
            ftx_message_rc_t unpack_status = ftx_message_decode(&message, &hash_if, text);
            if (unpack_status != FTX_MESSAGE_RC_OK)
            {
                snprintf(text, sizeof(text), "Error [%d] while unpacking!", (int)unpack_status);
            }
            num_decoded++;

            // Fake WSJT-X-like output for now
            float snr;
            snr = cand->score * 0.5f; // TODO: compute better approximation of SNR
            snr = ftx_get_snr(wf, cand);
            printf("%02d%02d%02d %+05.1f %+4.2f %4.0f ~  %s\n",
                tm_slot_start->tm_hour, tm_slot_start->tm_min, tm_slot_start->tm_sec,
                snr, time_sec, freq_hz, text);

        }
    }
    // Remove decoded candidate;
    ftx_delete_candidates(to_delete_idx, to_delete_size, candidate_list, num_candidates);
    return num_decoded;
}

int main(int argc, char** argv)
{
    // Accepted arguments
    const char* wav_path = NULL;
    const char* dev_name = NULL;
    ftx_protocol_t protocol = FTX_PROTOCOL_FT8;
    float time_shift = 0.8;

    // Parse arguments one by one
    int arg_idx = 1;
    while (arg_idx < argc)
    {
        // Check if the current argument is an option (-xxx)
        if (argv[arg_idx][0] == '-')
        {
            // Check agaist valid options
            if (0 == strcmp(argv[arg_idx], "-ft4"))
            {
                protocol = FTX_PROTOCOL_FT4;
            }
            else if (0 == strcmp(argv[arg_idx], "-list"))
            {
                audio_init();
                audio_list();
                return 0;
            }
            else if (0 == strcmp(argv[arg_idx], "-dev"))
            {
                if (arg_idx + 1 < argc)
                {
                    ++arg_idx;
                    dev_name = argv[arg_idx];
                }
                else
                {
                    usage("Expected an audio device name after -dev");
                    return -1;
                }
            }
            else
            {
                usage("Unknown command line option");
                return -1;
            }
        }
        else
        {
            if (wav_path == NULL)
            {
                wav_path = argv[arg_idx];
            }
            else
            {
                usage("Multiple positional arguments");
                return -1;
            }
        }
        ++arg_idx;
    }
    // Check if all mandatory arguments have been received
    if (wav_path == NULL && dev_name == NULL)
    {
        usage("Expected either INPUT file path or DEVICE name");
        return -1;
    }

    float slot_period = ((protocol == FTX_PROTOCOL_FT8) ? FT8_SLOT_TIME : FT4_SLOT_TIME);
    int sample_rate = 12000;
    int num_samples = slot_period * sample_rate;
    float signal[num_samples];

    int decode_block_stride = 10;
    int early_ldpc_iterations = 1;
    float find_candidates_at_frac = 0.7f;
    int find_candidates_at = (float)num_samples * find_candidates_at_frac;
    int num_decoded = 0;
    bool is_live = false;

    if (wav_path != NULL)
    {
        int rc = load_wav(signal, &num_samples, &sample_rate, wav_path);
        if (rc < 0)
        {
            LOG(LOG_ERROR, "ERROR: cannot load wave file %s\n", wav_path);
            return -1;
        }
        LOG(LOG_INFO, "Sample rate %d Hz, %d samples, %.3f seconds\n", sample_rate, num_samples, (double)num_samples / sample_rate);
    }
    else if (dev_name != NULL)
    {
        audio_init();
        audio_open(dev_name);
        num_samples = (slot_period - 0.4f) * sample_rate;
        is_live = true;
    }

    // Compute FFT over the whole signal and store it
    monitor_t mon;
    monitor_config_t mon_cfg = {
        .f_min = 200,
        .f_max = 3000,
        .sample_rate = sample_rate,
        .time_osr = kTime_osr,
        .freq_osr = kFreq_osr,
        .protocol = protocol
    };

    hashtable_init(256);

    monitor_init(&mon, &mon_cfg);
    LOG(LOG_DEBUG, "Waterfall allocated %d symbols\n", mon.wf.max_blocks);

    do
    {
        struct tm tm_slot_start = { 0 };
        if (is_live)
        {
            // Wait for the start of time slot
            while (true)
            {
                struct timespec spec;
                clock_gettime(CLOCK_REALTIME, &spec);
                double time = (double)spec.tv_sec + (spec.tv_nsec / 1e9);
                double time_within_slot = fmod(time - time_shift, slot_period);
                if (time_within_slot > slot_period / 4)
                {
                    audio_read(signal, mon.block_size);
                }
                else
                {
                    time_t time_slot_start = (time_t)(time - time_within_slot);
                    gmtime_r(&time_slot_start, &tm_slot_start);
                    LOG(LOG_INFO, "Time within slot %02d%02d%02d: %.3f s\n", tm_slot_start.tm_hour,
                        tm_slot_start.tm_min, tm_slot_start.tm_sec, time_within_slot);
                    break;
                }
            }
        }

        ftx_candidate_t candidate_list[kMax_candidates];
        int num_candidates = 0;

        // Hash table for decoded messages (to check for duplicates)
        int num_decoded = 0;
        ftx_message_t decoded[kMax_decoded_messages];
        ftx_message_t* decoded_hashtable[kMax_decoded_messages];

        // Initialize hash table pointers
        for (int i = 0; i < kMax_decoded_messages; ++i)
        {
            decoded_hashtable[i] = NULL;
        }

        // Process and accumulate audio data in a monitor/waterfall instance
        int block_n = 0;
        for (int frame_pos = 0; frame_pos + mon.block_size <= num_samples; frame_pos += mon.block_size)
        {
            block_n = (block_n + 1) % decode_block_stride;
            if (dev_name != NULL)
            {
                audio_read(signal + frame_pos, mon.block_size);
            }
            // LOG(LOG_DEBUG, "Frame pos: %.3fs\n", (float)(frame_pos + mon.block_size) / sample_rate);
            // fprintf(stderr, "#");
            // Process the waveform data frame by frame - you could have a live loop here with data from an audio device
            monitor_process(&mon, signal + frame_pos);

            if (frame_pos > find_candidates_at) {
                if (block_n == 0) {
                    if (num_candidates == 0) {
                        num_candidates = find_candidates(&mon, candidate_list, kMax_candidates);
                    } else {
                        num_decoded += decode_messages(&mon, &num_candidates, candidate_list, decoded, decoded_hashtable, early_ldpc_iterations, &tm_slot_start);

                    }
                }
            }
        }
        decode_messages(&mon, &num_candidates, candidate_list, decoded, decoded_hashtable, kLDPC_iterations, &tm_slot_start);
        fprintf(stderr, "\n");
        LOG(LOG_DEBUG, "Waterfall accumulated %d symbols\n", mon.wf.num_blocks);
        LOG(LOG_INFO, "Max magnitude: %.1f dB\n", mon.max_mag);

        LOG(LOG_INFO, "Decoded %d messages, callsign hashtable size %d\n", num_decoded, hashtable_get_size());
        hashtable_cleanup(10);
        // Reset internal variables for the next time slot
        monitor_reset(&mon);
    } while (is_live);

    monitor_free(&mon);

    return 0;
}
