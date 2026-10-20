# esp32-mqtt-qos-ack-publisher — publish sensor readings over MQTT and track delivery with sequence numbers and an ack topic

IoT example by [Fluidwire](https://fluidwire.com/).

Sample sketch for an [ESP32 dev board](https://fluidwire.com/) that adds **application-level delivery confirmation** on top of MQTT. Each reading carries a sequence number; the device waits for the backend to echo that number on an ack topic, retries on timeout, and prints delivery statistics — handy for measuring packet loss in thesis testbeds on weak WiFi.

## How it works

1. Device publishes `{"seq":N,"uptime_s":...,"rssi":...}` to `<base>/data`.
2. Backend (or a test script) replies with `N` on `<base>/ack`.
3. No ack within 3 s → resend, up to 3 retries, then drop and count it.
4. Serial output shows sent / acked / retried / dropped counters.

## What you need

- 1x ESP32 dev board
- MQTT broker + WiFi
- Something that acks (script below, Node-RED, or your own service)

## Required library

- **PubSubClient** by Nick O'Leary (Library Manager)

## Setup

1. Copy `config.h.example` → `config.h`, fill in WiFi + broker.
2. Flash. Serial Monitor at **115200 baud**.
3. Run a quick acker on your PC:

```bash
mosquitto_sub -h broker.hivemq.com -t 'fluidwire/demo/qosack/data' | while read l; do
  s=$(echo "$l" | sed 's/.*"seq":\([0-9]*\).*/\1/')
  mosquitto_pub -h broker.hivemq.com -t fluidwire/demo/qosack/ack -m "$s"
done
```

## Expected serial output

```
[wifi] IP: 192.168.1.50
[mqtt] connecting... ok
[tx] {"seq":1,"uptime_s":3,"rssi":-58} (attempt 1)
[ack] seq=1 confirmed after 112 ms
[stats] sent=1 acked=1 retried=0 dropped=0
```

## Troubleshooting

- **Always retrying / dropped**: nothing is publishing acks, or the ack topic doesn't match `MQTT_BASE_TOPIC`.
- **Stuck at "connecting"**: check broker host/port; public brokers sometimes rate-limit.
- **Duplicate data on the backend**: expected after retries — dedupe by `seq`.

## Notes

- MQTT QoS 1 only confirms delivery to the *broker*; this pattern confirms the *consumer* processed it.
- Publisher is stop-and-wait (one message in flight) for simplicity. Use a window of sequence numbers for higher throughput.
- `seq` resets on reboot; add a boot counter if you need global uniqueness.

## Security note

Never commit your real `config.h` (it holds WiFi and broker credentials). The included `.gitignore` excludes it. Public brokers like HiveMQ are unauthenticated — don't send private data.

## License

MIT — see [LICENSE](LICENSE).

## Got stuck?

Drop by [fluidwire.com](https://fluidwire.com/) — happy to help.
