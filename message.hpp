/*
 *     WeFish Message Type
 *
 *  Author: <hoky.guan@tymphany.com>
 *
 */
#ifndef MESSAGE_H
#define MESSAGE_H

typedef enum MessageType_ {
    MESSAGE_TYPE_CONTENT = 1,
    MESSAGE_TYPE_SETTING,
    MESSAGE_TYPE_MAX
} MessageType;

typedef enum NotificationType_ {
    NOTIFICATION_TYPE_HIS = 0,
    NOTIFICATION_TYPE_NO_HIS,
    NOTIFICATION_TYPE_MAX
} NotificationType;


#endif
