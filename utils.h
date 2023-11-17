struct topic {
    char name[51];
    int sf;
    int length;
    char content[1552];
    int ip;
    int port;
};

struct subscriber {
    char id[11];
    int fd;
    // struct topic *subscribed_topics;
    struct topic subscribed_topics[20];
    int no_subscribed_topics;
    // struct topic *missed_topics;
    struct topic missed_topics[20];
    int no_missed_topics;
};
