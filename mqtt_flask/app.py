from flask import Flask, render_template, Response
import cv2
import paho.mqtt.client as mqtt

app = Flask(__name__,static_folder='static')

MqttBroker = "broker.MQTTGO.io"
MqttPort = 1883
SubTopic1 = "mqtt/dashi/video"

# 設定連線成功時的Callback
def on_connect(client, userdata, flags, rc):
    print("連線結果：" + str(rc))
    # 訂閱主題
    client.subscribe(SubTopic1)

# 設定訂閱更新時的Callback
def on_message(client, userdata, msg):
    f = open('static/receive.jpg', 'wb+')  # 開啟檔案
    f.write(msg.payload)  # 寫入檔案
    f.close()  # 關閉檔案
    print('image received')

# 設定Mqtt連線
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(MqttBroker, MqttPort, 60)


@app.route('/')
def index():
    return render_template('index.html')


def gen():
    while True:
        client.loop()
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + open('static/receive.jpg', 'rb').read() + b'\r\n\r\n')


@app.route('/video_feed')
def video_feed():
    return Response(gen(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')


if __name__ == '__main__':
    app.run(debug=True)
