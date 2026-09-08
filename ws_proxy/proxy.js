const WebSocket = require('ws');
const https = require('https');
const crypto = require('crypto');
const fs = require('fs');
const zlib = require('zlib');
const protobuf = require('protobufjs');

// 配置
const PROXY_PORT = 8090;
const DOUYIN_WSS = 'wss://webcast100-ws-web-lq.douyin.com/webcast/im/push/v2/';

// 加载 protobuf
let root = null;
let PushFrame = null;
let Response = null;
let Message = null;
let ChatMessage = null;
let GiftMessage = null;
let MemberMessage = null;
let LikeMessage = null;
let SocialMessage = null;
let ControlMessage = null;

async function loadProto() {
    root = await protobuf.load(__dirname + '/../backend/proto/douyin.proto');
    PushFrame = root.lookupType('PushFrame');
    Response = root.lookupType('Response');
    Message = root.lookupType('Message');
    ChatMessage = root.lookupType('ChatMessage');
    GiftMessage = root.lookupType('GiftMessage');
    MemberMessage = root.lookupType('MemberMessage');
    LikeMessage = root.lookupType('LikeMessage');
    SocialMessage = root.lookupType('SocialMessage');
    ControlMessage = root.lookupType('ControlMessage');
    console.log('Protobuf loaded successfully');
}

// 获取 ttwid
function getTtwid() {
    return new Promise((resolve, reject) => {
        const req = https.request({
            hostname: 'live.douyin.com',
            path: '/',
            method: 'GET',
            headers: { 'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36 Edg/140.0.0.0' }
        }, (res) => {
            const cookies = res.headers['set-cookie'] || [];
            for (const c of cookies) {
                if (c.includes('ttwid=')) {
                    resolve(c.split('ttwid=')[1].split(';')[0]);
                    return;
                }
            }
            reject(new Error('ttwid not found'));
        });
        req.on('error', reject);
        req.end();
    });
}

// 获取 room_id
function getRoomId(liveId, ttwid) {
    return new Promise((resolve, reject) => {
        const req = https.request({
            hostname: 'live.douyin.com',
            path: '/' + liveId,
            method: 'GET',
            headers: {
                'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36 Edg/140.0.0.0',
                'Cookie': 'ttwid=' + ttwid
            }
        }, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                const match = data.match(/roomId\\":\\"(\d+)/);
                if (match) resolve(match[1]);
                else reject(new Error('roomId not found'));
            });
        });
        req.on('error', reject);
        req.end();
    });
}

// 生成签名
function generateSignature(params) {
    const code = fs.readFileSync(__dirname + '/../backend/scripts/sign.js', 'utf8');
    eval(code);
    return get_sign(params);
}

// 构建 WSS URL
function buildWssUrl(roomId) {
    const userId = String(Math.floor(Math.random() * 9e18));
    const now = Date.now();
    const traceId = crypto.randomBytes(4).toString('hex');

    const params = [
        'live_id=1', 'aid=6383', 'version_code=180800',
        'webcast_sdk_version=1.0.14-beta.0', 'room_id=' + roomId,
        'sub_room_id=', 'sub_channel_id=', 'did_rule=3',
        'user_unique_id=' + userId, 'device_platform=web',
        'device_type=', 'ac=', 'identity=audience'
    ].join(',');

    const md5 = crypto.createHash('md5').update(params).digest('hex');
    const signature = generateSignature(md5);

    return DOUYIN_WSS + '?' + [
        'aid=6383', 'app_name=douyin_web',
        'browser_language=zh-CN', 'browser_name=Mozilla',
        'browser_online=true', 'browser_platform=Win32',
        'browser_version=5.0%20(Windows%20NT%2010.0;%20Win64;%20x64)%20AppleWebKit/537.36%20(KHTML,%20like%20Gecko)%20Chrome/140.0.0.0%20Safari/537.36%20Edg/140.0.0.0',
        'compress=gzip', 'cookie_enabled=true',
        'cursor=d-1_u-1_fh-' + userId + '_t-' + now + '_r-1',
        'device_platform=web', 'device_type=', 'did_rule=3',
        'endpoint=live_pc', 'heartbeatDuration=0',
        'host=https://live.douyin.com', 'identity=audience',
        'im_path=/webcast/im/fetch/',
        'internal_ext=internal_src:dim|wss_push_room_id:' + roomId + '|wss_push_did:' + userId + '|dim_log_id:' + traceId,
        'live_id=1', 'need_persist_msg_count=15',
        'room_id=' + roomId, 'screen_height=864', 'screen_width=1536',
        'sub_channel_id=', 'sub_room_id=', 'support_wrds=1',
        'tz_name=Asia/Shanghai', 'update_version_code=1.0.14-beta.0',
        'user_unique_id=' + userId, 'version_code=180800',
        'webcast_sdk_version=1.0.14-beta.0',
        'signature=' + signature
    ].join('&');
}

// 解析消息
function parseMessage(msg) {
    const method = msg.method;
    const payload = msg.payload;

    try {
        switch (method) {
            case 'WebcastChatMessage': {
                const chat = ChatMessage.decode(payload);
                return {
                    type: 'chat',
                    user_id: String(chat.user?.userId || chat.user?.id || ''),
                    user_name: chat.user?.nickName || chat.user?.nickname || '',
                    content: chat.content || '',
                    timestamp: chat.common?.createTime || Date.now()
                };
            }
            case 'WebcastGiftMessage': {
                const gift = GiftMessage.decode(payload);
                return {
                    type: 'gift',
                    user_id: String(gift.user?.userId || gift.user?.id || ''),
                    user_name: gift.user?.nickName || gift.user?.nickname || '',
                    content: gift.gift?.name || gift.giftName || '',
                    gift_count: gift.comboCount || gift.repeatCount || 1,
                    timestamp: gift.common?.createTime || Date.now()
                };
            }
            case 'WebcastMemberMessage': {
                const member = MemberMessage.decode(payload);
                return {
                    type: 'enter',
                    user_id: String(member.user?.userId || member.user?.id || ''),
                    user_name: member.user?.nickName || member.user?.nickname || '',
                    content: '进入直播间',
                    timestamp: member.common?.createTime || Date.now()
                };
            }
            case 'WebcastLikeMessage': {
                const like = LikeMessage.decode(payload);
                return {
                    type: 'like',
                    user_id: String(like.user?.userId || like.user?.id || ''),
                    user_name: like.user?.nickName || like.user?.nickname || '',
                    content: '点赞了',
                    count: like.count || 1,
                    timestamp: like.common?.createTime || Date.now()
                };
            }
            case 'WebcastSocialMessage': {
                const social = SocialMessage.decode(payload);
                return {
                    type: 'social',
                    user_id: String(social.user?.userId || social.user?.id || ''),
                    user_name: social.user?.nickName || social.user?.nickname || '',
                    content: '关注了主播',
                    timestamp: social.common?.createTime || Date.now()
                };
            }
            case 'WebcastControlMessage': {
                const control = ControlMessage.decode(payload);
                return {
                    type: 'system',
                    content: control.status === 3 ? '直播已结束' : '系统消息',
                    timestamp: Date.now()
                };
            }
            case 'WebcastRoomUserSeqMessage': {
                return {
                    type: 'online_count',
                    content: '在线人数更新',
                    timestamp: Date.now()
                };
            }
            default:
                return null;
        }
    } catch (e) {
        console.log('Parse error for ' + method + ':', e.message);
        return null;
    }
}

// 解析帧
function parseFrame(data) {
    try {
        // 解析 PushFrame
        const frame = PushFrame.decode(data);

        // 心跳响应
        if (frame.payloadType === 'hb') {
            return [];
        }

        // gzip 解压
        const decompressed = zlib.gunzipSync(frame.payload);

        // 解析 Response
        const response = Response.decode(decompressed);

        const messages = [];

        // 遍历消息
        for (const msg of response.messagesList) {
            const parsed = parseMessage(msg);
            if (parsed) {
                parsed.room_id = '';
                messages.push(parsed);
            }
        }

        return messages;
    } catch (e) {
        return [];
    }
}

// 主代理服务器
async function main() {
    console.log('WebSocket Proxy starting...');

    await loadProto();

    const ttwid = await getTtwid();
    console.log('ttwid obtained');

    // 本地 WebSocket 服务器
    const wss = new WebSocket.Server({ port: PROXY_PORT });
    console.log('Proxy listening on ws://localhost:' + PROXY_PORT);

    wss.on('connection', (clientWs) => {
        console.log('Client connected to proxy');

        let douyinWs = null;
        let heartbeat = null;
        let roomId = '';

        clientWs.on('message', (data) => {
            const msg = data.toString();
            if (msg.startsWith('connect:')) {
                const liveId = msg.substring(8);
                console.log('Connecting to room:', liveId);

                getRoomId(liveId, ttwid).then(rid => {
                    roomId = rid;
                    console.log('Room ID:', roomId);
                    const wssUrl = buildWssUrl(roomId);

                    douyinWs = new WebSocket(wssUrl, {
                        headers: {
                            'Cookie': 'ttwid=' + ttwid,
                            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36 Edg/140.0.0.0'
                        }
                    });

                    douyinWs.on('open', () => {
                        console.log('Connected to Douyin!');
                        clientWs.send(JSON.stringify({ type: 'connected', room_id: roomId }));

                        // 心跳
                        heartbeat = setInterval(() => {
                            if (douyinWs && douyinWs.readyState === WebSocket.OPEN) {
                                douyinWs.send('hb');
                            }
                        }, 5000);
                    });

                    douyinWs.on('message', (data) => {
                        // 解析 protobuf 消息
                        console.log('Received message from Douyin, size:', data.length);
                        const messages = parseFrame(data);
                        console.log('Parsed messages:', messages.length);

                        for (const parsed of messages) {
                            parsed.room_id = roomId;
                            console.log('Sending message:', parsed.type, parsed.user_name);
                            // 发送解析后的 JSON 消息
                            if (clientWs.readyState === WebSocket.OPEN) {
                                clientWs.send(JSON.stringify(parsed));
                            }
                        }
                    });

                    douyinWs.on('error', (err) => {
                        console.log('Douyin WS error:', err.message);
                        clientWs.send(JSON.stringify({ type: 'error', message: err.message }));
                    });

                    douyinWs.on('close', () => {
                        console.log('Douyin WS closed');
                        if (heartbeat) clearInterval(heartbeat);
                        clientWs.send(JSON.stringify({ type: 'disconnected' }));
                    });
                }).catch(err => {
                    console.log('Error getting room ID:', err.message);
                    clientWs.send(JSON.stringify({ type: 'error', message: err.message }));
                });
            } else if (msg === 'disconnect') {
                if (douyinWs) {
                    douyinWs.close();
                    douyinWs = null;
                }
                if (heartbeat) {
                    clearInterval(heartbeat);
                    heartbeat = null;
                }
            }
        });

        clientWs.on('close', () => {
            console.log('Client disconnected');
            if (douyinWs) douyinWs.close();
            if (heartbeat) clearInterval(heartbeat);
        });
    });
}

main().catch(console.error);
