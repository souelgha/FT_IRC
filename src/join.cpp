#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"


void    Client::commandJoin(Server &server, std::string const &parameter)
{

    try {
        if (this->registered == false)
        {
            server.sendTemplate(*this, ERR_NOTREGISTERED(server.getName(), "*"));
            server.clearClient(this);
            return;
        }
    }
    catch (std::exception &e) {
        throw ;
    }

    std::istringstream  datas(parameter);
    std::string         channelName;
    std::string         keyvalue;

    datas >> channelName;
    datas >> keyvalue;

    if (channelName [0] != '#')
        channelName = '#' + channelName;

    if (server.getChannels().find(channelName) == server.getChannels().end())
        server.createChannel(*this, channelName, keyvalue);

    std::map<std::string, Channel *>::iterator  channelIt = server.getChannels().find(channelName);
    if (channelIt == server.getChannels().end())
    {
        server.sendTemplate(*this, ERR_NOSUCHCHANNEL(this->serverName, this->nickName, channelName));
        return ;
    }
    Channel *channel = channelIt->second;
    try {
        if(channel->getLMode() && channel->getUsers().size() >=channel->getLimitUsers())
            server.sendTemplate(*this, ERR_CHANNELISFULL(this->serverName, this->nickName, channel->getName()));
        else if (channel->getIMode() && !channel->isInvited(this->nickName))
            server.sendTemplate(*this, ERR_INVITEONLYCHAN(this->serverName, this->nickName, channel->getName()));
        else if (!channel->getIMode() && channel->getKMode() && (keyvalue == "" || keyvalue != channel->getKey()))
            server.sendTemplate(*this, ERR_BADCHANNELKEY(this->serverName, this->nickName, channel->getName()));
        else
        {
            if (channel->isUser(this->getNickName()))
                return ;
            channel->addUser(this);
            server.replyJoin(*this, *channel);
            listChannels.push_back(channel);
        }
    }
    catch (std::exception &e) {
        throw ;
    }
}

void    Server::replyJoin(Client const &client, Channel &channel) {

    {
        for (size_t i = 0; i < channel.getUsers().size(); i++)
            sendTemplate(*channel.getUsers()[i], RPL_JOIN(client.getSourceName(), channel.getName()));
    }

    {
        std::string message = RPL_NAMREPLY(client.getServerName(), client.getNickName(), channel.getName());

        for (size_t i = 0; i < channel.getUsers().size(); i++)
        {
            if (channel.isOperator(channel.getUsers()[i]->getNickName()))
                message += "@";
            message += channel.getUsers()[i]->getNickName() + " ";
        }
        message += CRLF;
        sendTemplate(client, message);
    }

    {
        std::string message = RPL_ENDOFNAMES(client.getServerName(), client.getNickName(), channel.getName());
        sendTemplate(client, message);
    }

}
