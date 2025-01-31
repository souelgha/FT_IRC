#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"

void    Server::replyModeClient(Client const &client) {

    sendTemplate(client, RPL_YOURHOST(client.getServerName(), client.getNickName()));
    sendTemplate(client, RPL_UMODEIS(client.getServerName(), client.getNickName(), client.getMode()));
}

void    Server::replyModeChannel(Client const &client, Channel &channel, std::string const &mode) {

    std::string message = "";

    if (!mode.empty())
    {
        channel.applyMode();   
        message = RPL_CHANNELMODEIS(client.getSourceName(), channel.getName(), mode);
    }
    else
        message = RPL_CHANNELMODE(client.getServerName(), client.getNickName(), channel.getName(), mode);

    for (size_t i = 0; i < channel.getUsers().size(); i++)
        sendTemplate(*channel.getUsers()[i], message);
}

void    Channel::insertNewMode(Server &server, Client &client, char sign, char sent, std::istringstream &parameter) {

    std::string key;
    std::string value;

    key = std::string(1, sign) + sent;
    if (sent == 'i' || sent == 'k' || sent == 'l' || sent == 'o' || sent == 't')
    {
        for (std::vector<std::pair<std::string, std::string> >::iterator it = mode.begin(); it != mode.end(); it++)
        {
            if (it->first[1] == sent)
                return ;
        }
    }
    if (sent == 'k' || sent == 'o')
    {
        parameter >> value;
        if ((sign == '+' && (value.empty()))
            || (sign == '-' && (sent == 'k' && value.empty() && this->oldkey.empty())))
            return ;
        if (sent == 'o' && !this->isUser(value))
        {
            server.sendTemplate(client, ERR_USERNOTINCHANNEL(client.getServerName(), client.getNickName(), this->name, value));
            if (!server.isClient(value))
                server.sendTemplate(client, ERR_NOSUCHNICK(client.getServerName(), client.getNickName(), value));
            return ;
        }
        if (sign == '-' && sent == 'k')
        {
            value = this->oldkey;
        }
    }
    else if (key == "+l")
    {
        parameter >> value;
        if (value.empty())
        {
            server.sendTemplate(client, ERR_NEEDMOREPARAMS(client.getServerName(), client.getNickName(), key));
            return ;
        }
        else if (std::atoi(value.c_str()) == 0)
            return ;
    }
    else if (sent == 'i' || sent == 't' || key == "-l")
        value = "";
    mode.push_back(std::make_pair(key, value));
}

void    Channel::adjustMode(Server &server, Client &client, std::string &value) {

    std::string         keys;
    size_t              i = 0;
    char                sign;

    if (value.empty() || (value[0] != '+' && value[0] != '-'))
        return ;

    std::istringstream  parameter(value);
    parameter >> keys;
    while (keys[i])
    {
        if (keys[i] == '-')
            sign = '-';
        else
            sign = '+';
        i++;
        for (; keys[i] && keys[i] != '+' && keys[i] != '-'; i++)
        {
            if (keys[i] != 'i' && keys[i] != 'k' && keys[i] != 'l' && keys[i] != 'o' && keys[i] != 't')
                server.sendTemplate(client, ERR_UNKNOWNMODE(client.getServerName(), client.getNickName(), keys[i], this->name));
            else
                insertNewMode(server, client, static_cast<char>(sign), keys[i], parameter);
        }
    }
}

void    Client::commandMode(Server &server, std::string const &parameter) {

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

    std::istringstream                      datas(parameter);
    std::string                             recipient;
    std::string                             value;


    datas >> recipient;
    std::getline(datas >> std::ws, value);

    try {
        if (server.getChannels().find(recipient) == server.getChannels().end())
        {
            if (this->mode.empty())
            {
                this->setMode("+i");
                server.replyModeClient(*this);
            }
            else
                server.sendTemplate(*this, ERR_NOSUCHCHANNEL(this->serverName, this->nickName, recipient));
        }
        else
        {
            if (recipient[0] != '#')
                recipient = "#" + recipient;
            Channel *channel = server.getChannels()[recipient];
            if (value.empty())
            {
                channel->setStringMode();
                server.sendTemplate(*this, RPL_CHANNELMODE(this->serverName, this->nickName, channel->getName(), channel->getStringMode()));
            }
            else if (value == "b")
            {
                server.sendTemplate(*this, RPL_ENDOFBANLIST(this->serverName, this->nickName, channel->getName()));
                return ;
            }
            else if (channel->isOperator(this->nickName))
            {
                channel->adjustMode(server, *this, value);
                server.replyModeChannel(*this, *channel, channel->modeToSend());
            }
            else
                server.sendTemplate(*this, ERR_CHANOPRIVSNEEDED(this->serverName, this->nickName, channel->getName()));
            channel->clearMode();
        }
    }
    catch (std::exception &e) {
        throw ;
    }
}

void    Channel::applyMode(void) 
{    
   std::vector<std::pair<std::string, std::string> >::iterator it;
    for (it = mode.begin() ; it != mode.end(); it++)
    {
        if(it->first == "+i" || it->first == "-i")
            modeI(it);
        else if(it->first == "+t" || it->first == "-t")
            modeT(it);
        else if(it->first == "+k" || it->first == "-k")
            modeKey(it);
        else if(it->first == "+l" || (it->first == "-l"))
            modeL(it);
        else if(it->first == "+o" || it->first == "-o")
            modeO(it);
    }
}

std::vector<std::pair<std::string, std::string> > const    &Channel::getMode() const {

    return (this->mode);
}

void Channel:: modeKey(std::vector<std::pair<std::string, std::string> >::iterator &it) 
{
    if(it->first == "+k")
    {
        if(it->second != "")
        {
            this->key = it->second;
            this->oldkey = it->second;
            this->kMode = true;
        }        
    }        
    else if(it->first == "-k")
    {        
        this->key = "";
        this->kMode = false;
    }
}

void Channel:: modeL(std::vector<std::pair<std::string, std::string> >::iterator &it) 
{
    if(it->first == "+l" && it->second!= "")
    {
        char const *val = ((it->second).c_str());
        this->limitUsers = std::atoi(val);
        this->lMode = true;
    }        
    else if(it->first == "-l") 
    {
        this->limitUsers = 10000;
        this->lMode = false;
        
    }
    
}

void Channel:: modeI(std::vector<std::pair<std::string, std::string> >::iterator &it) 
{
    if(it->first == "+i")
        this->iMode = true;       
    else if(it->first == "-i")
    {
        this->iMode = false;
        this->invited.clear();
    }
}

void Channel:: modeT(std::vector<std::pair<std::string, std::string> >::iterator &it) 
{
    if(it->first == "+t")
    {
        this->tMode = true;
    }        
    else if(it->first == "-t")
    {
        this->tMode = false;
    }
    
}

void    Channel::modeO(std::vector<std::pair<std::string, std::string> >::iterator &it) 
{
    if(it->first == "+o" && it->second != "")
    {  
        for (size_t i = 0; i < users.size(); i++)
        {
            if(users[i]->getNickName() == it->second)
            {
                addOperator(it->second); 
                break;
            }
        }    
    }        
    else if(it->first == "-o" && it->second != "")
    {
        if(operators.find(it->second) != operators.end())
            remOperator(it->second);
    }    
}

std::string const   Channel::modeToSend(void) {

    if (this->mode.empty())
        return ("");

    std::vector<std::pair<std::string, std::string> >::iterator    it = this->mode.begin();
    std::string                                     mode = it->first;
    char                                            sign = it->first[0];

    it++;
    for (; it != this->mode.end(); it++)
    {
        if (it->first[0] == sign)
            mode += it->first[1];
        else
        {
            mode += it->first;
            sign = it->first[0];
        }
    }
    for (it = this->mode.begin(); it != this->mode.end(); it++)
    {
        if (!it->second.empty())
            mode += " " + it->second;
    }
    return(mode);
}

void    Channel::clearMode(void) {

    this->mode.clear();
}
