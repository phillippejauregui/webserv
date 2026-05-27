/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationConfig.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lylrandr <lylrandr@student.42lausanne.ch>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/27 18:26:25 by lylrandr          #+#    #+#             */
/*   Updated: 2026/05/14 14:17:31 by lylrandr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

# include <vector>
# include <string>
# include <map>
# include <iostream>

struct LocationConfig {
	bool								upload_enabled;
	bool								autoindex;
	std::string							upload_store;
	std::string							path;
	std::string							root;
	std::string							index;
	std::string							redirect;
	std::vector<std::string>			methods;
	std::map<std::string, std::string>	cgi;
};

#endif
