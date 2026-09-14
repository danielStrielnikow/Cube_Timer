package pl.danielstrielnikow.cube_backend.repository;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;
import pl.danielstrielnikow.cube_backend.model.Cube;
import pl.danielstrielnikow.cube_backend.model.WorkSession;

import java.util.Optional;
import java.util.UUID;

@Repository
public interface WorkSessionRepository extends JpaRepository<WorkSession, UUID> {
    Optional<WorkSession> findFirstByCubeAndEndTimeIsNullOrderByStartTimeDesc(Cube cube);
}
