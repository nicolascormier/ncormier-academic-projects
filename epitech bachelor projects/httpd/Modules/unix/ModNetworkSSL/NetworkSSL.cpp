/*
**
**	$HeadURL: svn://epizia.homeip.net/repo/zia/src/Network.cpp $
**	$Author: nico $
**	$Revision: 300 $
**	$Date: 2005-11-24 12:06:19 +0100 (Thu, 24 Nov 2005) $
**
*/

#include <iostream>
#include <errno.h>
#include <sstream>

#include "NetworkSSL.h"

#include "Actions.h"
#include "DBuffer.h"
#include "Exceptions.h"
#include "TicketsHandler.h"


using namespace std;
using namespace mozia::ticket;
using namespace mozia::exceptions;

namespace mozia
{
  namespace network
  {
    NetworkSSL::NetworkSSL(TicketsHandler *th)
    {
      this->_th = th;
      this->_nbrSocketSet = 0;
    }

    NetworkSSL::~NetworkSSL(void)
    {
    }

    int NetworkSSL::receiveFromClient(ClientSocketSSL *client)
    {
//       int ret = read(client->getSocket(), this->_ibuf, BUFSIZE);
      int ret = SSL_read(client->_sslsocket, this->_ibuf, BUFSIZE);

      if (ret <= 0)
	return (-1);
      client->getWBuf().addData(this->_ibuf, ret);
      return (ret);
    }

    int NetworkSSL::sendToClient(ClientSocketSSL *client)
    {
      if (client->getSBuf().getSize() == 0)
	return (0);

//       int ret = write(client->getSocket(), client->getSBuf().getData(),
// 		      client->getSBuf().getSize());
      int ret = SSL_write(client->_sslsocket, client->getSBuf().getData(),
			  client->getSBuf().getSize());
      if (ret <= 0)
	{
	  this->closeClient(client);
	  if (client->_isFlagSet(CLIENT_HASTICKET) == true)
	    this->_th->Drop("/dev/null", CORE_TICKET_DELETED);
	  else
	    this->_toDelete.push_front(client);
	  return (-1);
	}
      return (ret);
    }

    void	NetworkSSL::dropTicketRead(ClientSocketSSL *client)
    {
      //
      // On transfere les donnees du buffer temporaire vers le buffer client
      //
      Ticket *t = client->getTicket();

      client->getBuf().addData(client->getWBuf());
      client->getWBuf().Clear();
      t->CData = &(client->getBuf());
      this->_th->CurTicket = t;
      client->_unsetFlag(CLIENT_HASTICKET);
      this->_th->Drop(this->_th->CurTicket->GetFrom(), NEWDATA);
    }

    void	NetworkSSL::dropTicketWrite(ClientSocketSSL* client)
    {
      if (client->getTicket()->CData->GetType() == "Buffer")
	{
	  Buffer* buf = (Buffer*) client->getTicket()->CData;
	  //buf->Clear();
	  // YBR Modif
	  buf->rmData(buf->getSize());
	}
      this->_th->CurTicket = client->getTicket();
      client->_unsetFlag(CLIENT_HASTICKET);
      this->_th->Drop(client->getTicket()->GetFrom(), NEWBUFFER);
    }


    bool	NetworkSSL::isBadClient(ClientSocketSSL *client)
    {
      for (list<ClientSocketSSL *>::iterator iter = this->_toDelete.begin();
	   iter != this->_toDelete.end(); iter++)
	if (*iter == client)
	  {
	    this->_toDelete.erase(iter);
	    return (true);
	  }
      return (false);
    }

    void	NetworkSSL::treatTickets(void)
    {
      if (!this->_th->TryGet())
	return;

      if (this->_th->CurTicket->GetAction() == CORE_HALT_MODULE)
	{
	  this->_th->Drop("CORE", CORE_HALTED_MODULE);
	  return ;
	}

      ClientSocketSSL *tmp = (ClientSocketSSL *) this->_th->CurTicket->GetMyData();

      if (this->isBadClient(tmp))
	{
	  tmp->_unsetFlag(CLIENT_HASTICKET);
	  this->_th->Drop("/dev/null", CORE_TICKET_DELETED);
	  return;
	}

      if (this->_th->CurTicket->GetFrom() == "CORE" &&
	  this->_th->CurTicket->GetAction() == CORE_DELETE_TICKET)
	{
	  this->closeAndDropClient(tmp);
	  return;
	}

      if (this->_th->CurTicket->CData == NULL)
	{
	  if ( (this->_th->CurTicket->GetAction() == NETWORK_READ ||
		this->_th->CurTicket->GetAction() == NETWORK_CLEAN )
	       && tmp != NULL)
	    {
	      this->_th->CurTicket->CData = &(tmp->getBuf());
	      if (this->_th->CurTicket->GetAction() == NETWORK_CLEAN)
		((Buffer*) this->_th->CurTicket->CData)->Clear();
	    }
	  else
	    {
	      this->closeAndDropClient(tmp);
	      return;
	    }
	}
      if (this->_th->CurTicket->CData->GetType() == "ClientSocketSSL")
	{
	  //
	  // New clients from listener
	  //
	  this->_th->CurTicket->SetMyData((IModuleData*)((ClientSocketSSL*) this->_th->CurTicket->CData));
	  this->_clients[((ClientSocketSSL*) this->_th->CurTicket->CData)->getSocket()] =
	    (ClientSocketSSL*) this->_th->CurTicket->CData;
	  ((ClientSocketSSL*) this->_th->CurTicket->CData)->_setFlag(CLIENT_HASTICKET);
	  //
	  // Make the socket non-blocking and
	  // associate it with network events
	  //
	  // Useless because of WSAEventSelect.
	  //
	  /*
	    int iMode = 1;
	    if (ioctlsocket(((ClientSocketSSL*) this->_th->CurTicket->CData)->getSocket(),
	    FIONBIO, (u_long FAR*) &iMode) == SOCKET_ERROR)
	    {
	    this->closeAndDropClient(tmp);
	    return;
	    }
	  */
	  this->_nbrSocketSet++;

	  // ADD to select read fds

	  //if (nRet == SOCKET_ERROR)
	  //this->closeAndDropClient(tmp);
	  return;
	}

      if (tmp == NULL)
	{
	  this->closeAndDropClient(tmp);
	  return;
	}

      //
      // /!\	A partir d'ici tmp est considere comme existant et bon	/!\
	  //
	  if (this->_th->CurTicket->CData->GetType() != "Buffer")
	    {
	      this->closeAndDropClient(tmp);
	      return;
	    }

	  //
	  // /!\  CData forcement == Buffer a partir d'ici
	  //
	  tmp->_setFlag(CLIENT_HASTICKET);
	  switch (tmp->getTicket()->GetAction())
	    {
	    case (NETWORK_SENDDATA):
	      {
		Sock_CltSock_map::iterator pos;
		Buffer* buf = (Buffer*) tmp->getTicket()->CData;

		if (buf->getSize() == 0)
		  {
		    this->closeAndDropClient(tmp);
		    return;
		  }

		if (!tmp->getSBuf().getSize())
		  {
		    //
		    // Aucun buffer en attente, on cree
		    //
		    tmp->getSBuf().setData(*buf);
		    tmp->_setFlag(CLIENT_HASTICKET);
		  }
		else
		  {
		    //
		    // Un buffer deja en attente, on fusionne les 2
		    //
		    tmp->getSBuf().addData(*buf);
		  }
		this->dropTicketWrite(tmp);
		this->_toSend.push(tmp);
		return;
	      }
	      break;
	    }
	  //
	  // Si on a recu un ticket et qu'on a des data en attente pour lui,
	  // on send direct
	  //
	  if (tmp->getBuf().getSize() > 0)
	    this->dropTicketRead(tmp);
    }

    void	NetworkSSL::closeClient(ClientSocketSSL *client)
    {
      this->_nbrSocketSet--;
      this->_clients.erase(client->getSocket());
    }


    void	NetworkSSL::closeAndDropClient(ClientSocketSSL* client)
    {
      if (client)
	{
	  this->closeClient(client);
	  client->_unsetFlag(CLIENT_HASTICKET);
	}
      this->_th->Drop("/dev/null", CORE_TICKET_DELETED);
    }

    void	NetworkSSL::treatFD(void)
    {
      Sock_CltSock_map::iterator	iter;

      for (iter = this->_clients.begin(); iter != this->_clients.end(); )
	{
	  ClientSocketSSL *client = iter->second;
	  iter++;

	  if (FD_ISSET(client->getSocket(), &this->_readfds))
	    {
	      if (this->receiveFromClient(client) != -1)
		{
		  if (client->_isFlagSet(CLIENT_HASTICKET) == true)
		    this->dropTicketRead(client);
		}
	      else
		{
		  this->closeClient(client);
		  if (client->_isFlagSet(CLIENT_HASTICKET) == true)
		    {
		      this->_th->CurTicket = client->getTicket();
		      this->_th->Drop("/dev/null", CORE_TICKET_DELETED);
		    }
		  else
		    this->_toDelete.push_front(client);
		}
	    }
	}
    }

    void	NetworkSSL::sendStack()
    {
      while (!this->_toSend.empty())
	{
	  ClientSocketSSL*	client = this->_toSend.top();
	  this->_toSend.pop();

	  int ret = this->sendToClient(client);
	  if (ret == -1)
	    continue;
	  if (ret == client->getSBuf().getSize())
	    client->getSBuf().Clear();
	  else
	    client->getSBuf().Trunc(client->getSBuf().getSize() - ret);
	}
    }

    void  NetworkSSL::mainLoop(void)
    {
      while (1)
	{
	  int nfds = -1;
	  struct timeval timeout = {0, 10};

	  FD_ZERO(&this->_readfds);
	  FD_ZERO(&this->_writefds);

	  Sock_CltSock_map::iterator	iter;

	  for (iter = this->_clients.begin(); iter != this->_clients.end(); )
	    {
	      ClientSocketSSL *client = iter->second;
	      iter++;
	      if (nfds < client->getSocket())
		nfds = client->getSocket();
	      FD_SET(client->getSocket(), &this->_readfds);
	    }

	  if (select(nfds + 1, &this->_readfds, &this->_writefds, NULL,
		     &timeout) == -1)
	    {
	      ostringstream oss;
	      oss << "NetworkSSL::mainLoop()  errno=" << ::strerror(errno);
	      throw new NetworkException(oss.str(), errno);
	    }
	  this->treatTickets();
	  this->sendStack();
	  this->treatFD();
	}
    }
  }
}
