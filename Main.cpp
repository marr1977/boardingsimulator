#include <SFML/Graphics.hpp>
#include <algorithm>
#include <random>
#include <iostream>
#include <vector>
#include <chrono>
#include <cstdint>

//#define DEBUG_OUTPUT_DRAW
//#define DEBUG_OUTPUT_BOARDING

class ModelParameters
{
    std::default_random_engine randomEngine;
    std::normal_distribution<float> passengerSpeedDistribution;
    std::normal_distribution<float> waitDurationDistribution;

public:
    ModelParameters() : 
        randomEngine(std::chrono::system_clock::now().time_since_epoch().count()),
        passengerSpeedDistribution(10 /* mean */, 3 /* std dev */),
        waitDurationDistribution(7 /* mean */, 3 /* std dev */)
    {
 
    }

    std::default_random_engine& GetRandomNumberEngine() { return randomEngine; }

    float GetPassengerSpeed()
    {
        return std::max(5.0f, passengerSpeedDistribution(randomEngine)) * GetScaleFactor();
    }

    float GetWaitDuration()
    {
        return std::min(std::max(1.0f, waitDurationDistribution(randomEngine)), 15.0f) /  GetScaleFactor();
    }

    float GetScaleFactor()
    {
        return 20.f;
    }
};

class Seat : public sf::RectangleShape
{
private:
    bool isOccupied;
public:
    static constexpr float GetSeatWidth() { return 10; }
    static constexpr float GetSeatHeight() { return 10; }


    Seat() : isOccupied(false)
    {
        setSize(sf::Vector2f(GetSeatWidth(), GetSeatHeight()));
    }

    void Draw(sf::RenderTarget& target, float x, float y)
    {
        setFillColor(isOccupied ? sf::Color::Green : sf::Color::White);
        setPosition(x, y);
        target.draw(*this);
    }

    void SetOccupied()
    {
        isOccupied = true;
    }

};

class Passenger : public sf::CircleShape
{
private:
    int row;
    int seat;
    float isleY;
    float yVelocity;
    
    bool hasMovedOnce = false;
    bool isSeated = false;
    bool isBoarded = false;
    bool isMoving = true;
    Seat* myseat;

    std::chrono::time_point<std::chrono::system_clock> lastMove;
    std::chrono::time_point<std::chrono::system_clock> waitStart;
    double waitDuration;

public:
    static constexpr float GetRadius() { return 5; }

    Passenger(int _row, int _seat, float _yVelocity, float _waitDuration, Seat* _myseat) : 
        row(_row), seat(_seat), isleY(0), yVelocity(_yVelocity), myseat(_myseat), waitDuration(_waitDuration)
    {
        std::cout << "Created passenger at " << row << " " << seat << " with velocity " << yVelocity << " and wait duration " << waitDuration << std::endl;
        setFillColor(sf::Color::Green);
        setRadius(GetRadius());
    }

    bool IsBoarded() const { return isBoarded; }
    bool IsSeated() const { return isSeated; }
    int GetRow() const { return row; }
    int GetSeat() const { return seat; }
    void SetBoarded() { isBoarded = true; }
    void SetSeated() { isSeated = true; }
    bool InIsle() const { return isBoarded && !isSeated; }
    float GetVelocity() const { return yVelocity; }

    void Draw(sf::RenderTarget& target, const sf::Vector2f& isleOrigin, std::vector<Passenger>& passengers)
    {
        if (!InIsle())
        {
            return;
        }
        
#ifdef DEBUG_OUTPUT_DRAW
        auto i = reinterpret_cast<std::uintptr_t>(this);
        std::cout << "Drawing passenger at " << row << " " << seat << " " << i << " with isleY " << isleY << ", origin y is " << isleOrigin.y << std::endl;
#endif
        std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

        if (isMoving)
        {
            if (hasMovedOnce)
            {
                std::chrono::duration<double> diff = now - lastMove;
                float yToMove = diff.count() * yVelocity;
                
                // Cap yToMove so that we don't run over another passenger
                for (const auto& passenger : passengers) 
                {
                    if (&passenger == this) 
                    {
                        continue;
                    }

                    if (!passenger.InIsle()) 
                    {
                        continue;
                    }

                    float distanceToPassenger = passenger.getPosition().y - getPosition().y;

                    if (distanceToPassenger < 0) 
                    {
                        continue;
                    }

                    float spaceBetweenUs = distanceToPassenger - getRadius() * 2 - 1 /* buffer */;
                    if (spaceBetweenUs < yToMove)
                    {
                        yToMove = spaceBetweenUs;
                    }
                }

                isleY += yToMove;

                if (isleOrigin.y + isleY > myseat->getPosition().y)
                {
                    isleY = myseat->getPosition().y - isleOrigin.y;
                    waitStart = now;
                    isMoving = false;
                }
            } 

            setPosition(isleOrigin.x + (16.f /* isle width */ / 2) - getRadius(), isleOrigin.y + isleY);
            lastMove = now;
            hasMovedOnce = true;
        }
        else
        {
            std::chrono::duration<double> diff = now - waitStart;
            if (diff.count() >= waitDuration)
            {
                //std::cout << "Sitting down after waiting " << diff.count() << ", wait duration was " << waitDuration << std::endl;
                myseat->SetOccupied();
                isSeated = true;
            }
        }

        target.draw(*this);

    }

};


class Plane
{
private:
    ModelParameters params;
    std::vector<std::vector<Seat>> seats;
    int noRows;
    int noSeats;
    std::vector<Passenger> passengers;

    sf::Vector2f isleOrigin;

public:
    Plane(ModelParameters& _params, int _noRows, int _noSeats) : params(_params), noRows(_noRows), noSeats(_noSeats)
    {
        for (int i = 0; i < noRows; i++) 
        {
            seats.emplace_back(noSeats);

            for (int j = 0; j < noSeats; j++) 
            {
                passengers.emplace_back(i, j, 
                params.GetPassengerSpeed(), 
                params.GetWaitDuration(), 
                &(seats.at(i).at(j)));
            }
        }
    }

    const float START_Y = 50;
    const float ROW_SPACING = 10;
    const float SEAT_SPACING = 6;

    static constexpr float GetIsleWidth() { return 16; }

    void Draw(sf::RenderTarget& target)
    {
        float seatStartX = 300;
        float seatY = START_Y;
        float seatX;

        //
        // Draw seats
        //
        for (int row = 0; row < seats.size(); row++) 
        {
            seatX = seatStartX;
            std::vector<Seat>& rowSeats = seats.at(row);

            for (int seatNo = 0; seatNo < rowSeats.size(); seatNo++)
            {
                Seat& seat = rowSeats.at(seatNo);
                seat.Draw(target, seatX, seatY);

                seatX += Seat::GetSeatWidth();

                if (seatNo == (rowSeats.size() / 2 - 1)) 
                {
                    if (row == 0 && (isleOrigin.x != seatX || isleOrigin.y != seatY)) {
                        isleOrigin = sf::Vector2f(seatX, seatY);
                    }

                    seatX += GetIsleWidth();
                }
                else
                {
                    seatX += SEAT_SPACING;
                }
            }

            seatY += ROW_SPACING + Seat::GetSeatHeight();
        }

        //
        // Draw passengers
        //
        for (auto& passenger : passengers)
        {
            passenger.Draw(target, isleOrigin, passengers);
        }
    }

    bool Board(Passenger& passenger)
    {
        if (!CanBoard())
        {
            return false;
        }
    
        std::cout << "Passenger at " << passenger.GetRow() << " " << passenger.GetSeat() << " boarded with velocity " << passenger.GetVelocity() << std::endl;
    
        passenger.SetBoarded(); 
        passenger.setPosition(sf::Vector2f(isleOrigin.x, isleOrigin.y));
        
    }

    bool CanBoard() const
    {
        if (isleOrigin.x == 0 && isleOrigin.y == 0)
        {
            return false;
        }

        float lowestPassengerY = std::numeric_limits<float>::max();

        for (const auto& passenger : passengers)
        {
            if (!passenger.InIsle())
            {
                continue;
            }

            auto i = reinterpret_cast<std::uintptr_t>(&passenger);

#ifdef DEBUG_OUTPUT_BOARDING
            std::cout << "Passenger at " << " (" << passenger.GetRow() << " " << passenger.GetSeat() << ") " << i << " has y pos " << passenger.getPosition().y  << std::endl;
#endif
            lowestPassengerY = std::min(lowestPassengerY, passenger.getPosition().y);

#ifdef DEBUG_OUTPUT_BOARDING      
            std::cout << "Lowest passenger y so far is " << lowestPassengerY << " at " << passenger.getPosition().y << " (" << passenger.GetRow() << " " << passenger.GetSeat() << ")" << std::endl;
#endif
        }

        float space = lowestPassengerY - START_Y;
        
#ifdef DEBUG_OUTPUT_BOARDING
        std::cout << "Space for boarding is " << space << ", radius is " << Passenger::GetRadius() << std::endl;
#endif

        return space >= 2 * Passenger::GetRadius() + 1;
    }

    std::vector<Passenger>& GetPassengerList()
    {
        return passengers;
    }
};

void RandomizePassengerList(std::default_random_engine& rng, std::vector<Passenger>& list)
{
    std::shuffle(std::begin(list), std::end(list), rng);
}

void SortBySection(int numRows, int numSections, std::vector<Passenger>::iterator begin, std::vector<Passenger>::iterator end)
{
    if (numSections == 1)
        return;

    int sectionSize = numRows / numSections;
    std::sort(begin, end, [sectionSize](const Passenger&a, const Passenger&b) -> bool {
        int aSection = a.GetRow() / sectionSize;
        int bSection = b.GetRow() / sectionSize;
        return aSection > bSection;
    });
}

int main()
{

    ModelParameters params;

    sf::RenderWindow window(sf::VideoMode(1024, 768), "Boarding simulator");
    sf::CircleShape shape(100.f);
    shape.setFillColor(sf::Color::Black);

    int numRows = 20;
    int numSeats = 4;
    int numSections = 8;

    Plane plane(params, numRows, numSeats);

    RandomizePassengerList(params.GetRandomNumberEngine(), plane.GetPassengerList());

    std::cout << "Sorting by section" << std::endl;
    
    bool sortBySection = false;

    if(sortBySection)
    {
        SortBySection(numRows, numSections, plane.GetPassengerList().begin(), plane.GetPassengerList().end());
    }
    else
    {
        int peoplePerSection = 3;
        std::vector<Passenger>::iterator begin = plane.GetPassengerList().begin();
        while (true)
        {
            std::vector<Passenger>::iterator end = begin + peoplePerSection * numSections;

            if (end > plane.GetPassengerList().end())
            {
                end = plane.GetPassengerList().end();
            }
            SortBySection(numRows, numSections, begin, end);

            if (end == plane.GetPassengerList().end())
                break;

            begin = end;
        }
    }

    std::cout << "Passenger list:" << std::endl;
    for (const auto& p : plane.GetPassengerList())
    {
        std::cout << p.GetRow() << " " << p.GetSeat() << std::endl;
    }


    
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

    int nextPassenger = 0;
    bool allPassengersSeated;

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        window.clear();
        
        plane.Draw(window);

        if (nextPassenger < plane.GetPassengerList().size() && plane.Board(plane.GetPassengerList().at(nextPassenger))) {
            nextPassenger++;
        }

        if (!allPassengersSeated && nextPassenger == plane.GetPassengerList().size())
        {
            allPassengersSeated = true;
            for (const auto& passenger : plane.GetPassengerList())
            {
                if (!passenger.IsSeated())
                {
                    allPassengersSeated = false;
                    break;
                }
            }

            if (allPassengersSeated)
            {
                std::chrono::duration<double> seatingDuration = std::chrono::system_clock::now() - start;
                std::cout << "BOARDING COMPLETED. TOOK " << seatingDuration.count() * params.GetScaleFactor() << " SECONDS" << std::endl;
                window.close();
                return 1;
            }
        }

        
        window.display();
    }

    return 0;
}