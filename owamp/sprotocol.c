/*
**      $Id$
*/
/************************************************************************
*									*
*			     Copyright (C)  2002			*
*				Internet2				*
*			     All Rights Reserved			*
*									*
************************************************************************/
/*
**	File:		cprotocol.c
**
**	Author:		Jeff W. Boote
**			Anatoly Karp
**
**	Date:		Wed Apr  24 10:42:12  2002
**
**	Description:	This file contains the private functions that
**			speak the owamp protocol directly from the
**			server point of view.
**			(i.e. read and write the data and save it
**			to structures for the rest of the api to deal
**			with.)
**
**			The idea is to basically keep all network ordering
**			architecture dependant things in this file.
*/
#include <owampP.h>

void
owp_block_decrypt(char *a, char* b, char* c, int d, char* e);
void
_OWPServerOK(OWPControl ctrl, u_int8_t code);

int
_OWPSendServerGreeting(
		       OWPControl cntrl,          /* cntrl state structure  */
		       OWPErrSeverity	*err_ret  /* error - returned       */
		       )
{
	char buf[MAX_MSG];
	int encrypt = 0;
	u_int32_t mode = cntrl->mode;	  /* modes available */

	/* first 16 bytes: unused + advertised mode */
	memset(buf, 0, sizeof(buf));
	*(int32_t *)(buf + 12) = htonl(mode); /* first 12 bytes unused */

	/* generate 16 random bytes and save them away. */
	random_bytes(cntrl->challenge, 16);
	memcpy(buf + 16, cntrl->challenge, 16); /* the last 16 bytes */

	if (send_blocks(cntrl->sockfd, buf, 2, encrypt) < 0){
		close(cntrl->sockfd);
		return -1; 
	}
	
	return 0;
}

int
_OWPReadClientGreeting(
		       OWPControl cntrl, 
		       void* app_data
		       )
{
	u_int32_t mode_offered = cntrl->mode;
	u_int32_t mode_requested; 
	char buf[MAX_MSG];
	OWPErrSeverity err_ret;
	char token[32];

	if (readn(cntrl->sockfd, buf, 60) != 60){
		close(cntrl->sockfd);
		return -1;
	}

	mode_requested = ntohl(*(u_int32_t *)buf);
	
	/*
	     XXX - TODO: improve logic of handling mode_requested.
	*/

	if (mode_requested & ~mode_offered){ /* can't provide requested mode */
		_OWPServerOK(cntrl, CTRL_REJECT);
		close(cntrl->sockfd);
		return -1;
	}
	
	if (mode_requested & OWP_MODE_AUTHENTICATED){
		memcpy(cntrl->kid, buf + 4, 8); /* Save 8 bytes of kid */

		(*cntrl->ctx->cfg.get_aes_key_func)(app_data, buf + 4, 
					       cntrl->key, &err_ret);
		/* if all ok - set up key + cipher now 
		     XXX - TODO
		*/

		/* Decrypt two 16-byte blocks - save the result into token.*/
		owp_block_decrypt(cntrl->readIV, cntrl->key, 
				  buf + 12, 2*(16*8), token);
		
		/* Decrypted challenge is in the first 16 bytes */
		if (memcmp(cntrl->challenge, token, 16) != 0){
			_OWPServerOK(cntrl, CTRL_REJECT);
			close(cntrl->sockfd);
			return -1;
		}

		/* Save 16 bytes of session key and 16 bytes of client IV*/
		memcpy(cntrl->session_key, token + 16, 16);
		memcpy(cntrl->readIV, buf + 44, 16);
	}

	/* Apparently everything is ok. Accept the Control session. */
	cntrl->mode = mode_requested;
	_OWPServerOK(cntrl, CTRL_ACCEPT);
	return 0;

	/*
	      XXX - TODO: make sure all fields off cntrl are set.
	*/
}

/*
** Accept or reject the Control Connection request.
** Code = CTRL_ACCEPT/CTRL_REJECT with the obvious meaning.
*/
void
_OWPServerOK(OWPControl ctrl, u_int8_t code)
{
	char buf[MAX_MSG];

	memset(buf, 0, 32);
	*(u_int8_t *)(buf+15) = code;
	if (code == 0){ /* accept */
		random_bytes(buf + 16, 16); /* Generate Server-IV */
		/* XXX - TODO
		   OWPSetWriteIV(ctrl, buf + 16); 
		*/
	}
	send_blocks(ctrl->sockfd, buf, 2, 0);
}

void
owp_block_decrypt(char *a, char* b, char* c, int d, char* e)
{
	;
}