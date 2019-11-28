var admin = require('firebase-admin');
admin.initializeApp({
    credential: admin.credential.applicationDefault(),
});
module.exports = async function (context, IoTHubMessages) {
    context.log(`JavaScript eventhub trigger function called for message array:`
        + `${JSON.stringify(IoTHubMessages)}`);

    IoTHubMessages.forEach(async message => {
        context.log(`Processed message: ${JSON.stringify(message)}`);

        const defaultExpiry = 48 * 3600;  // in seconds (hours * hours_per_second)
        var androidExpiration = defaultExpiry * 1000;  // in milliseconds
        var appleExpiration = Math.floor((new Date()).getTime() / 1000)
                                + defaultExpiry;  // in UTC time

        var messageEvent = 'event';
        var confidence = message['confidence'] * 100;
        var eventTime = new Date(message['eventTime'] * 1000).toISOString().substr(11, 8);
        switch (message['eventType']) {
            case 'window_break':
                messageEvent = "breaking window";
                break;
            case 'gunshot':
                messageEvent = "gunshot";
        }
        messageText = `A ${messageEvent} was detected at ${eventTime} UTC `
            + `with confidence ${confidence.toFixed(0)}%.`;
        context.log(`Notification text: ${messageText}`);

        var notificationMessage = {
            data: {
                "click_action": "FLUTTER_NOTIFICATION_CLICK"
            },
            'notification': {
                'body': messageText,
                'title': "Security Event"
            },
            'topic': 'events',
            'android': {
                "ttl": androidExpiration,
                "priority": "high",
                'notification': {
                    'sound': 'default'
                }
            },
            'apns': {
                "headers": {
                    "apns-expiration": appleExpiration.toString()
                },
                'payload': {
                    'aps': {
                        'sound': "default"
                    }
                }
            }
        };

        // Send a message to devices subscribed to the provided topic.
        try {
            var response = await admin.messaging().send(notificationMessage);
            context.log('Successfully sent message:', response);
        } catch (error) {
            context.log('Error sending message:', error);
        }
    });
    context.done();
};